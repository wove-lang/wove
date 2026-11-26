# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Lower the first Wove ownership forms to C11."""

from collections.abc import Iterator
from contextlib import contextmanager
from dataclasses import dataclass, field
import os
from pathlib import Path
import re

from syntax import IDENTIFIER, Atom, Form, Node, SourceError, form, identifier, parse


I8 = "i8"
I32 = "i32"
U8 = "u8"
U32 = "u32"
BOOL = "bool"
VOID = "void"
RUNTIME_INCLUDE = '"wove_runtime.h"'
HEADER_PATH = re.compile(r"[A-Za-z0-9_./-]+")
INTEGER_LITERAL = re.compile(r"-?(?:0[xX][0-9a-fA-F]+|[0-9]+)")
INTEGER_SUFFIX = re.compile(rf"({INTEGER_LITERAL.pattern})([iu][0-9]+)")
SCALAR_LIMITS = {
	I8: (-128, 127),
	I32: (-(2**31), 2**31 - 1),
	U8: (0, 255),
	U32: (0, 2**32 - 1),
}
SCALAR_TYPES = (*SCALAR_LIMITS, BOOL)
OWNED = "owned-array"
SHARED = "shared-array"
MUT = "mut-array"
C_TYPE = {
	I8: "int8_t",
	I32: "int32_t",
	U8: "uint8_t",
	U32: "uint32_t",
	BOOL: "bool",
	VOID: "void",
	OWNED: "wvrt_array *",
	SHARED: "wvrt_slice",
	MUT: "wvrt_mut_slice",
}
WRAPPING_OPS = {
	"i32.add": (I32, "wvrt_i32_add_wrapping"),
	"i32.sub": (I32, "wvrt_i32_sub_wrapping"),
	"u32.add": (U32, "wvrt_u32_add_wrapping"),
	"u32.sub": (U32, "wvrt_u32_sub_wrapping"),
}
STEP_OPS = {"inc": "add", "dec": "sub"}
BINARY_OPS = {
	"u8.eq": (U8, BOOL, "=="),
	"u8.not-eq": (U8, BOOL, "!="),
	"u8.lt": (U8, BOOL, "<"),
	"u8.not-lt": (U8, BOOL, ">="),
	"i32.eq": (I32, BOOL, "=="),
	"i32.not-eq": (I32, BOOL, "!="),
	"i32.lt": (I32, BOOL, "<"),
	"i32.not-lt": (I32, BOOL, ">="),
	"u32.eq": (U32, BOOL, "=="),
	"u32.not-eq": (U32, BOOL, "!="),
	"u32.lt": (U32, BOOL, "<"),
	"u32.not-lt": (U32, BOOL, ">="),
	"u32.and": (U32, U32, "&"),
	"u32.or": (U32, U32, "|"),
	"u32.xor": (U32, U32, "^"),
}
ZERO_PREDICATES = {
	"eqz": ("eq", False),
	"not-eqz": ("not-eq", False),
	"ltz": ("lt", False),
	"not-ltz": ("not-lt", False),
	"gtz": ("lt", True),
	"not-gtz": ("not-lt", True),
}
U32_SHIFT_OPS = {
	"u32.shl": "<<",
	"u32.shr": ">>",
}


@dataclass(frozen=True)
class RecordField:
	kind: str
	c_name: str


# Record equality follows the defining declaration, including across file scopes.
@dataclass(eq=False, frozen=True)
class Record:
	c_name: str
	fields: dict[str, RecordField]


@dataclass(frozen=True)
class BufferType:
	access: str
	element: str | Record


Type = str | Record | BufferType


_BYTE_VIEW = BufferType(SHARED, U8)
_BYTE_OUTPUT = BufferType(MUT, U8)
# Only the private bootstrap host ABI bypasses ordinary Wove name lookup.
BOOTSTRAP_CALLS: dict[str, tuple[tuple[Type, ...], Type]] = {
	"bootstrap_canonical_path": ((_BYTE_VIEW, _BYTE_OUTPUT), I32),
	"bootstrap_file": ((_BYTE_VIEW, _BYTE_OUTPUT), I32),
	"bootstrap_same_file": ((_BYTE_VIEW, _BYTE_VIEW), BOOL),
	"bootstrap_write": ((_BYTE_VIEW, BOOL), BOOL),
	"bootstrap_os": ((), U8),
	"bootstrap_arch": ((), U8),
}


@dataclass(frozen=True)
class Value:
	kind: Type
	c: str


@dataclass(eq=False)
class Binding:
	value: Value
	writable: bool = False


@dataclass(frozen=True)
class ModuleValue:
	binding: Binding
	initializer: str | bytes


@dataclass(frozen=True)
class Function:
	c_name: str
	params: tuple[tuple[str, Type], ...]
	result: str | Record
	body: Form
	host_span: Record | None = None

	# The host adapter owns the application entry.
	@property
	def is_entry(self) -> bool:
		return self.host_span is not None


@dataclass(frozen=True)
class Module:
	filename: str
	function_index: int
	records: dict[str, Record] = field(default_factory=dict)
	function_declarations: dict[str, Form] = field(default_factory=dict)
	uses: list[Form] = field(default_factory=list)
	type_bindings: dict[str, Record] = field(default_factory=dict)
	functions: dict[str, Function] = field(default_factory=dict)
	bindings: dict[str, Function] = field(default_factory=dict)
	values: dict[str, ModuleValue] = field(default_factory=dict)
	value_bindings: dict[str, ModuleValue] = field(default_factory=dict)


# Scalars and scalar-only records can be passed without a loan or transfer.
def copyable(kind: Type) -> bool:
	return isinstance(kind, Record) or kind in SCALAR_TYPES


# Arrays and slices share descriptor layouts while values retain their native types.
def c_type(kind: Type) -> str:
	# The descriptor depends on the access mode and carries a typed element count.
	if isinstance(kind, BufferType):
		return C_TYPE[kind.access]
	return kind.c_name if isinstance(kind, Record) else C_TYPE[kind]


# Normalize a suffixed integer atom to the equivalent typed form.
def value_form(node: Node) -> Form:
	# Quoted strings and other atoms retain the ordinary form check.
	if isinstance(node, Atom) and not node.quoted:
		literal = INTEGER_SUFFIX.fullmatch(node.text)
		# Reuse the typed form's range check and C emission.
		if literal is not None and literal[2] in SCALAR_LIMITS:
			return Form(literal[2], (Atom(literal[1], node.line),), node.line)
	return checked_form(node)


# Validate a typed integer literal before emitting its C spelling.
def read_integer_literal(node: Form) -> Value:
	literal, = form(node, arity=1).operands
	# The grammar limits the radix, digits, and sign before Python parses the value.
	if not isinstance(literal, Atom) or literal.quoted or not INTEGER_LITERAL.fullmatch(literal.text):
		raise SourceError(node.line)
	low, high = SCALAR_LIMITS[node.tag]
	hexadecimal = literal.text.removeprefix("-").startswith(("0x", "0X"))
	# An unsigned hexadecimal literal cannot carry a minus, even for zero.
	if hexadecimal and low == 0 and literal.text.startswith("-"):
		raise SourceError(node.line)
	# Convert both radices to the same decimal C spelling.
	try:
		number = int(literal.text, 16 if hexadecimal else 10)
	except ValueError:
		raise SourceError(node.line) from None
	# Check the declared type before handing the literal to C.
	if not low <= number <= high:
		raise SourceError(node.line)
	spelling = f"UINT32_C({number})" if node.tag == U32 else str(number)
	return Value(node.tag, spelling)


# Read an immutable scalar or byte table without evaluating runtime expressions.
def read_module_value(node: Form, index: int) -> tuple[str, ModuleValue]:
	name_node, initializer = form(node, "let", 2).operands
	name = identifier(name_node)
	c_name = f"wv_seed_binding_{index}"
	# A quoted string supplies the exact bytes of a shared u8 slice.
	if isinstance(initializer, Atom) and initializer.quoted:
		# Reject string contents that cannot be represented as UTF-8.
		try:
			data = initializer.text.encode("utf-8")
		except UnicodeEncodeError:
			raise SourceError(initializer.line) from None
		# Slice lengths fit the signed i32 domain.
		if len(data) > SCALAR_LIMITS[I32][1]:
			raise SourceError(initializer.line)
		return name, ModuleValue(Binding(Value(BufferType(SHARED, U8), c_name)), data)
	literal = value_form(initializer)
	# Only the supported integer literals can initialize a module scalar.
	if literal.tag not in SCALAR_LIMITS:
		raise SourceError(initializer.line)
	value = read_integer_literal(literal)
	return name, ModuleValue(Binding(Value(value.kind, c_name)), value.c)


# Give each byte table one read-only backing array and a static shared view.
def module_value_definition(declaration: ModuleValue) -> list[str]:
	value = declaration.binding.value
	initializer = declaration.initializer
	# Scalars are stored in their declared C type.
	if isinstance(initializer, str):
		return [f"static const {c_type(value.kind)} {value.c} = {initializer};"]
	lines = []
	items = "NULL"
	# Empty views need no backing allocation.
	if initializer:
		items = f"{value.c}_bytes"
		bytes_c = ", ".join(str(byte) for byte in initializer)
		lines.append(f"static const uint8_t {items}[{len(initializer)}] = {{{bytes_c}}};")
	lines.append(
		f"static const wvrt_slice {value.c} = {{{items}, {len(initializer)}, sizeof(uint8_t)}};"
	)
	return lines


# Collect scalar fields in declaration order for the record's native layout.
def read_record(node: Form, index: int) -> tuple[str, Record]:
	name_node, fields_node = form(node, "struct", 2).operands
	name = identifier(name_node)
	# Keep named records distinct from builtin value and loan types.
	if name in C_TYPE:
		raise SourceError(name_node.line)
	fields = {}
	# Only scalar fields participate in the seed's copyable record layout.
	for field_index, field_node in enumerate(checked_form(fields_node, "fields").operands):
		field = checked_form(field_node, arity=1)
		kind_node, = field.operands
		kind = identifier(kind_node)
		# A field name is an ordinary identifier and selects exactly one member.
		if field.tag.startswith(":") or field.tag in fields:
			raise SourceError(field.line)
		# Owning and borrowed fields require record lifetime rules beyond the seed.
		if kind not in SCALAR_TYPES:
			raise SourceError(kind_node.line)
		fields[field.tag] = RecordField(kind, f"wv_field_{field_index}")
	return name, Record(f"wv_seed_record_{index}", fields)


# Give each named record a distinct C type with its fields in declaration order.
def record_definition(record: Record) -> list[str]:
	fields = [
		f"    {C_TYPE[field.kind]} {field.c_name};"
		for field in record.fields.values()
	]
	# C11 needs at least one member even when the Wove record has no fields.
	return ["typedef struct {", *(fields or ["    uint8_t wv_empty;"]), f"}} {record.c_name};", ""]


# Read a copyable input or a shared or mutable view of a supported element.
def parameter_type(node: Node, records: dict[str, Record]) -> Type:
	# Copyable types are written as bare symbols.
	if isinstance(node, Atom) and not node.quoted:
		kind = records.get(node.text, node.text)
		# Keep nominal record identity in the signature.
		if copyable(kind):
			return kind
	view = checked_form(node, arity=1)
	# Slice parameters specify a read or write loan.
	if view.tag not in (SHARED, MUT):
		raise SourceError(node.line)
	element = identifier(view.operands[0])
	# Keep the declared element type as part of the slice type.
	if element != U8 and element not in records:
		raise SourceError(node.line)
	return BufferType(view.tag, records.get(element, element))


# Require the same concrete argument-span layout as the native runtime adapter.
def read_host_span(params: dict[str, Type], line: int) -> Record:
	# The host supplies a shared byte arena followed by its shared span table.
	if len(params) != 2:
		raise SourceError(line)
	bytes_kind, spans_kind = params.values()
	# Validate the two view kinds before inspecting the span element record.
	if bytes_kind != BufferType(SHARED, U8) or not isinstance(spans_kind, BufferType):
		raise SourceError(line)
	record = spans_kind.element
	# Generated callbacks write the defining record directly, in declaration order.
	if spans_kind.access != SHARED or not isinstance(record, Record) or tuple(
		(name, field.kind) for name, field in record.fields.items()
	) != (("start", I32), ("end", I32)):
		raise SourceError(line)
	return record


# Read the signature before lowering calls, including forward calls.
def read_function(
	name: str, node: Form, index: int, records: dict[str, Record], *, entry_module: bool
) -> Function:
	fields = {}
	# Keep each tagged field at its original line for validation and lowering.
	for child in node.operands[1:]:
		field = form(child)
		# An unknown or repeated field is invalid at that occurrence.
		if field.tag not in ("params", "returns", "body") or field.tag in fields:
			raise SourceError(field.line)
		fields[field.tag] = field
	# An absent slot has no field location, so use the declaration line.
	if len(fields) != 3:
		raise SourceError(node.line)
	params_node, returns_node, body_node = (fields[tag] for tag in ("params", "returns", "body"))

	params = {}
	# Preserve parameter order for the generated C signature.
	for param in checked_form(params_node, "params").operands:
		field = checked_form(param, arity=1)
		# Each ordinary parameter name introduces a distinct binding.
		if field.tag.startswith(":") or field.tag in params:
			raise SourceError(param.line)
		params[field.tag] = parameter_type(field.operands[0], records)
	result_node, = checked_form(returns_node, "returns", 1).operands
	result_name = identifier(result_node)
	result = records.get(result_name, result_name)
	# A function returns a copyable value or completes without a value.
	if result != VOID and not copyable(result):
		raise SourceError(result_node.line)
	entry = entry_module and name == "main"
	# The application entry implements the shared host argument ABI.
	if entry and result != I32:
		raise SourceError(node.line)
	host_span = read_host_span(params, node.line) if entry else None

	c_name = f"wv_seed_fn_{index}"
	return Function(c_name, tuple(params.items()), result, checked_form(body_node, "body"), host_span)


# Encode a source filename as a C literal, including arbitrary path bytes.
def c_string(text: str) -> str:
	parts = ['"']
	# Keep the literal safe for C even when the filename contains special bytes.
	for byte in os.fsencode(text):
		# Print ordinary characters directly so source locations stay readable.
		if 32 <= byte <= 126 and byte not in b'"\\?':
			parts.append(chr(byte))
		# Use fixed-width escapes for quotes, trigraph characters, and nonprintable bytes.
		else:
			parts.append(f"\\{byte:03o}")
	parts.append('"')
	return "".join(parts)


# Give each parameter a C name independently of its source spelling.
def signature(function: Function) -> str:
	params = ", ".join(
		f"{c_type(kind)} wv_value_{index}"
		for index, (_, kind) in enumerate(function.params)
	)
	result = f"static {c_type(function.result)}"
	return f"{result} {function.c_name}({params or 'void'})"


class FunctionEmitter:
	# Initialize the function scope, C names, and ownership state.
	def __init__(self, function: Function, module: Module, source: str):
		self.function = function
		self.functions = module.bindings
		self.records = module.type_bindings
		self.values = module.value_bindings
		self.source = source
		self.lines = []
		self.scopes = [{
			name: Binding(Value(kind, f"wv_value_{index}"))
			for index, (name, kind) in enumerate(function.params)
		}]
		self.next_value = len(function.params)
		self.moved: set[Binding] = set()
		self.loans: list[tuple[Binding, str]] = []

	# Indent generated statements at the current lexical scope.
	def emit_line(self, line: str) -> None:
		self.lines.append("    " * len(self.scopes) + line)

	# Evaluate each operation in source order before emitting its consumer.
	def temporary(self, kind: Type, expression: str) -> Value:
		name = f"wv_value_{self.next_value}"
		self.next_value += 1
		self.emit_line(f"{c_type(kind)} {name} = {expression};")
		return Value(kind, name)

	# Resolve a live place and report failures at the operation using it.
	def local(self, node: Node, line: int) -> Binding:
		name, = checked_form(node, "local", 1).operands
		symbol = identifier(name)
		# The nearest declaration shadows any outer binding with the same name.
		for scope in reversed(self.scopes):
			binding = scope.get(symbol)
			# Stop at the first declaration so a moved shadow still hides the outer name.
			if binding is not None:
				# A move on any incoming path makes this place unavailable.
				if binding in self.moved:
					raise SourceError(line)
				return binding
		declaration = self.values.get(symbol)
		# A lexical lookup can fall back to a defining or selected module binding.
		if declaration is not None:
			return declaration.binding
		raise SourceError(line)

	# Require the exact result type at the operation that consumes it.
	def typed(self, node: Node, kind: Type) -> Value:
		value = self.expression(node)
		# Scalar arithmetic and calls need an explicit conversion between types.
		if value.kind != kind:
			raise SourceError(node.line)
		return value

	# Check an access against loans held during operand evaluation.
	def check_access(self, binding: Binding, exclusive: bool, line: int) -> None:
		# Shared access can coexist with other shared access to the same place.
		for parent, loan in self.loans:
			# Any exclusive access conflicts with an outstanding loan of that place.
			if parent is binding and (exclusive or loan == MUT):
				raise SourceError(line)

	# Resolve the binding and descriptor for a permitted array access.
	def array_access(self, node: Node, line: int, writable: bool = False) -> tuple[Binding, BufferType, str]:
		binding = self.local(node, line)
		value = binding.value
		kind = value.kind
		# Only array owners and views carry an element descriptor.
		if not isinstance(kind, BufferType):
			raise SourceError(line)
		owned = kind.access == OWNED
		# Writes require the owner's mutable binding or an exclusive parameter.
		if writable and not (binding.writable if owned else kind.access == MUT):
			raise SourceError(line)
		self.check_access(binding, writable, line)
		return binding, kind, value.c + ("->" if owned else ".")

	# Index a native record only after the runtime verifies the signed element index.
	def record_element(
		self, fields: str, element: str | Record, index: Value, line: int, *, writable: bool = False
	) -> str:
		native = c_type(element)
		pointer = native if writable else f"const {native}"
		offset = f"wvrt_slice_offset({fields}wvrt_length, {index.c}, {self.site(line)})"
		return f"(({pointer} *){fields}wvrt_items)[{offset}]"

	# Supply the source line to runtime checks emitted for this operation.
	def site(self, line: int) -> str:
		return f"(wvrt_location){{{self.source}, {line}, 1}}"

	# Hold this loan through the remaining argument evaluations and the call.
	def borrow(self, node: Node, kind: BufferType) -> Value:
		exclusive = kind.access == MUT
		tag = "borrow-mut" if exclusive else "borrow-shared"
		target_node, = checked_form(node, tag, 1).operands
		target = checked_form(target_node)
		source = target
		range_bounds = None
		# A range projects the named source for a byte-array call argument.
		if target.tag == "array.range":
			# Range projection uses the runtime byte stride.
			if kind.element != U8:
				raise SourceError(node.line)
			source, start, end = form(target, arity=3).operands
			range_bounds = (start, end)
		binding = self.local(source, node.line)
		origin = binding.value
		origin_kind = origin.kind
		# The loan and its source must agree on the exact element type.
		if not isinstance(origin_kind, BufferType) or origin_kind.element != kind.element:
			raise SourceError(node.line)
		owned = origin_kind.access == OWNED
		writable = binding.writable if owned else origin_kind.access == MUT
		# Exclusive access comes from a writable owner or a mutable slice type.
		if exclusive and not writable:
			raise SourceError(node.line)
		self.check_access(binding, exclusive, node.line)
		fields = origin.c + ("->" if owned else ".")

		# Evaluate both range endpoints before acquiring the new loan.
		if range_bounds is not None:
			start = self.typed(range_bounds[0], I32)
			end = self.typed(range_bounds[1], I32)
			helper = "wvrt_mut_slice_range_u8" if exclusive else "wvrt_slice_range_u8"
			expression = (
				f"{helper}({fields}wvrt_items, {fields}wvrt_length, "
				f"{start.c}, {end.c}, {self.site(target.line)})"
			)
		# A whole-place borrow keeps the original pointer and extent.
		else:
			expression = (
				f"({C_TYPE[kind.access]}){{{fields}wvrt_items, "
				f"{fields}wvrt_length, {fields}wvrt_element_size}}"
			)
		self.loans.append((binding, kind.access))
		return self.temporary(kind, expression)

	# Evaluate arguments left to right and end only this call's loans.
	def call(self, node: Form) -> Value:
		# A call needs a named callee before its arguments.
		if not node.operands:
			raise SourceError(node.line)
		name, *args = node.operands
		symbol = identifier(name)
		callee = self.functions.get(symbol)
		# The fixed host calls apply only when ordinary Wove lookup has no binding.
		if callee is None:
			bootstrap = BOOTSTRAP_CALLS.get(symbol)
			# Keep arbitrary C functions outside the seed's bootstrap surface.
			if bootstrap is None:
				raise SourceError(node.line)
			parameters, return_type = bootstrap
			c_name = f"wvrt_{symbol}"
		else:
			# Neither representation of the root entry is a callable Wove helper.
			if callee.is_entry:
				raise SourceError(node.line)
			parameters = tuple(kind for _, kind in callee.params)
			return_type = callee.result
			c_name = callee.c_name
		# Resolve the exact signature before evaluating any argument.
		if len(args) != len(parameters):
			raise SourceError(node.line)
		checkpoint = len(self.loans)
		values = []
		# Each argument may acquire a loan that constrains later arguments.
		for argument, kind in zip(args, parameters):
			value = self.borrow(argument, kind) if isinstance(kind, BufferType) else self.typed(argument, kind)
			values.append(value.c)
		expression = f"{c_name}({', '.join(values)})"
		# A void call is a statement and supplies no C value to a consumer.
		if return_type == VOID:
			self.emit_line(f"{expression};")
			result = Value(VOID, "")
		# Value-returning calls capture their result before the next operation.
		else:
			result = self.temporary(return_type, expression)
		del self.loans[checkpoint:]
		return result

	# Keep right-hand evaluation inside its branch while checking it unconditionally.
	def short_circuit(self, node: Form) -> Value:
		left, right = form(node, arity=2).operands
		first = self.typed(left, BOOL)
		result = self.temporary(BOOL, first.c)
		test = result.c if node.tag == "bool.and" else f"!{result.c}"
		# The left result survives when the right operand is skipped.
		self.emit_line(f"if ({test}) {{")
		# Boolean operands cannot move enclosing owners, so only C temporaries need a scope.
		self.scopes.append({})
		second = self.typed(right, BOOL)
		self.emit_line(f"{result.c} = {second.c};")
		self.scopes.pop()
		self.emit_line("}")
		return result

	# Evaluate field expressions in source order and assemble the named value.
	def make_record(self, node: Form) -> Value:
		# The constructor needs a type name before its field initializers.
		if not node.operands:
			raise SourceError(node.line)
		name, *fields = node.operands
		record = self.records.get(identifier(name))
		# Only a declared record has a usable member layout.
		if record is None:
			raise SourceError(name.line)
		initializers = {}
		# Resolve each initializer to its C member before evaluating its value.
		for field_node in fields:
			initializer = checked_form(field_node, arity=1)
			field = record.fields.get(initializer.tag)
			# A constructor supplies each declared member exactly once.
			if field is None or initializer.tag in initializers:
				raise SourceError(field_node.line)
			value = self.typed(initializer.operands[0], field.kind)
			initializers[initializer.tag] = f".{field.c_name} = {value.c}"
		# Require a complete record without implicit defaults for source fields.
		if len(initializers) != len(record.fields):
			raise SourceError(node.line)
		values = ", ".join(initializers.values()) or "0"
		return self.temporary(record, f"({record.c_name}){{{values}}}")

	# Read a scalar member from an evaluated record value.
	def record_field(self, node: Form) -> Value:
		target, name = form(node, arity=2).operands
		value = self.expression(target)
		record = value.kind
		# A field lookup needs the target's declared record type.
		if not isinstance(record, Record):
			raise SourceError(target.line)
		field = record.fields.get(identifier(name))
		# The declared type determines both the field's presence and scalar type.
		if field is None:
			raise SourceError(name.line)
		return self.temporary(field.kind, f"{value.c}.{field.c_name}")

	# Translate one operation, preserving ownership of every array value.
	def expression(self, node: Node) -> Value:
		expr = node = value_form(node)
		# A boolean literal carries the same type as a comparison result.
		if expr.tag == BOOL:
			literal, = form(expr, arity=1).operands
			spelling = identifier(literal)
			# Accept the two boolean spellings.
			if spelling not in ("true", "false"):
				raise SourceError(literal.line)
			return Value(BOOL, spelling)

		# Binary boolean expressions guard every operation in the right operand.
		if expr.tag in ("bool.and", "bool.or"):
			return self.short_circuit(expr)

		# Negation requires a boolean and keeps its type.
		if expr.tag == "bool.not":
			operand, = form(expr, arity=1).operands
			value = self.typed(operand, BOOL)
			return self.temporary(BOOL, f"!{value.c}")

		# Discard promoted bits above the exact width of the complemented word.
		if expr.tag == "u32.not":
			operand, = form(expr, arity=1).operands
			value = self.typed(operand, U32)
			return self.temporary(U32, f"(uint32_t)~(uint64_t){value.c}")

		# Function literals follow the same rules as module initializers.
		if expr.tag in SCALAR_LIMITS:
			return read_integer_literal(expr)

		# Match the complete typed alias using only the seed's current integer types.
		destination, separator, origin = expr.tag.partition(".from-")
		typed_conversion = bool(separator and destination in SCALAR_LIMITS and origin in SCALAR_LIMITS)
		if expr.tag == "int.convert" or typed_conversion:
			# Both spellings evaluate the same single operand and use the same range limits.
			if typed_conversion:
				operand, = form(expr, arity=1).operands
				kind = destination
				value = self.typed(operand, origin)
			else:
				target, operand = form(expr, arity=2).operands
				kind = identifier(target)
				# Check the destination before evaluating the operand.
				if kind not in SCALAR_LIMITS:
					raise SourceError(node.line)
				value = self.expression(operand)
				# The generic spelling accepts any seed integer as its source.
				if value.kind not in SCALAR_LIMITS:
					raise SourceError(node.line)
			# Retain the seed's existing direct widening lowering for this alias.
			if expr.tag == "i32.from-u8":
				return self.temporary(I32, f"(int32_t){value.c}")
			low, high = SCALAR_LIMITS[kind]
			return self.temporary(
				kind, f"({C_TYPE[kind]})wvrt_int_convert({value.c}, {low}, {high}, "
				f"{self.site(node.line)})"
			)

		# A record construction produces a new value of its declared type.
		if expr.tag == "struct.new":
			return self.make_record(expr)

		# A member access produces a copy of the declared scalar field.
		if expr.tag == "struct.get":
			return self.record_field(expr)

		# An ordinary value use copies a scalar or record binding.
		if expr.tag == "local":
			binding = self.local(expr, node.line)
			# Array and slice places need an explicit operation.
			if not copyable(binding.value.kind):
				raise SourceError(node.line)
			# Capture the value now even if a later statement writes to its slot.
			if binding.writable:
				return self.temporary(binding.value.kind, binding.value.c)
			return binding.value

		# Allocate one runtime array with the source location for traps.
		if expr.tag == "array.alloc":
			element_node, length = form(expr, arity=2).operands
			name = identifier(element_node)
			element = self.records.get(name, name)
			# The seed supports byte arrays and arrays of declared copyable records.
			if element != U8 and not isinstance(element, Record):
				raise SourceError(node.line)
			size = self.typed(length, I32)
			native = c_type(element)
			site = self.site(node.line)
			return self.temporary(
				BufferType(OWNED, element), f"wvrt_array_new({size.c}, sizeof({native}), _Alignof({native}), {site})"
			)

		# Transfer the pointer and invalidate the original owner slot.
		if expr.tag == "move":
			target, = form(expr, arity=1).operands
			binding = self.local(target, node.line)
			kind = binding.value.kind
			# Only an owner can transfer its allocation.
			if not isinstance(kind, BufferType) or kind.access != OWNED:
				raise SourceError(node.line)
			self.check_access(binding, True, node.line)
			result = self.temporary(kind, binding.value.c)
			self.moved.add(binding)
			self.emit_line(f"{binding.value.c} = NULL;")
			return result

		# Read the element count of the live owner or received view.
		if expr.tag == "array.len":
			target, = form(expr, arity=1).operands
			_, _, fields = self.array_access(target, node.line)
			return self.temporary(I32, f"{fields}wvrt_length")

		# Copy an element through either descriptor after checking its index.
		if expr.tag == "array.load":
			target, subscript = form(expr, arity=2).operands
			_, kind, fields = self.array_access(target, node.line)
			element = kind.element
			index = self.typed(subscript, I32)
			# A typed record access copies the entire element.
			if element != U8:
				return self.temporary(element, self.record_element(fields, element, index, node.line))
			return self.temporary(
				U8, f"wvrt_slice_load_u8({fields}wvrt_items, {fields}wvrt_length, "
				f"{index.c}, {self.site(node.line)})"
			)

		prefix, _, predicate = expr.tag.partition(".")
		step = STEP_OPS.get(predicate)
		wrapping = WRAPPING_OPS.get(f"{prefix}.{step}" if step is not None else expr.tag)
		# Evaluate exact-type operands in order before performing wrapping arithmetic.
		if wrapping is not None:
			kind, helper = wrapping
			# A unary step uses the existing arithmetic helper with an implicit one.
			if step is not None:
				operand, = form(expr, arity=1).operands
				value = self.typed(operand, kind)
				return self.temporary(kind, f"{helper}({value.c}, 1)")
			left, right = form(expr, arity=2).operands
			first = self.typed(left, kind)
			second = self.typed(right, kind)
			return self.temporary(kind, f"{helper}({first.c}, {second.c})")

		zero_comparison = ZERO_PREDICATES.get(predicate)
		# Reuse the same typed operators when a zero operand is implicit.
		if zero_comparison is not None:
			comparison, reverse = zero_comparison
			operation = BINARY_OPS.get(f"{prefix}.{comparison}")
			# A unary alias cannot introduce a type the binary seed does not support.
			if operation is not None:
				kind, result, operator = operation
				operand, = form(expr, arity=1).operands
				value = self.typed(operand, kind)
				# Keep the operand evaluation when unsigned comparison with zero is constant.
				if kind in (U8, U32) and predicate in ("ltz", "not-ltz"):
					truth = "true" if predicate == "not-ltz" else "false"
					return self.temporary(result, f"((void){value.c}, {truth})")
				left, right = ("0", value.c) if reverse else (value.c, "0")
				return self.temporary(result, f"{left} {operator} {right}")

		binary = BINARY_OPS.get(expr.tag)
		# Check both operand types before applying the comparison or mask.
		if binary is not None:
			kind, result, operator = binary
			left, right = form(expr, arity=2).operands
			first = self.typed(left, kind)
			second = self.typed(right, kind)
			return self.temporary(result, f"{first.c} {operator} {second.c}")

		shift = U32_SHIFT_OPS.get(expr.tag)
		# Widen the word before shifting and check the count at this form's location.
		if shift is not None:
			left, right = form(expr, arity=2).operands
			word = self.typed(left, U32)
			count = self.typed(right, I32)
			return self.temporary(
				U32, f"(uint32_t)((uint64_t){word.c} {shift} "
				f"wvrt_u32_shift_count({count.c}, {self.site(node.line)}))"
			)

		# Call lowering manages the lifetime of its argument loans.
		if expr.tag == "call":
			return self.call(expr)

		raise SourceError(node.line)

	# Release owned slots from the scopes being exited in reverse declaration order.
	def cleanup(self, scopes: list[dict[str, Binding]]) -> None:
		# Clean up inner scopes before outer ones.
		for scope in reversed(scopes):
			# An earlier declaration outlives a later one in the same scope.
			for binding in reversed(scope.values()):
				kind = binding.value.kind
				# A moved slot holds NULL on the path that moved it.
				if isinstance(kind, BufferType) and kind.access == OWNED:
					self.emit_line(f"wvrt_array_drop({binding.value.c});")

	# Evaluate the return value before releasing every scope the return leaves.
	def emit_return(self, node: Form) -> None:
		# A void return leaves the function without a value.
		if self.function.result == VOID:
			form(node, arity=0)
			exit_line = "return;"
		# Evaluate the result before dropping the scopes it can read.
		else:
			operand, = form(node, arity=1).operands
			value = self.typed(operand, self.function.result)
			exit_line = f"return {value.c};"
		self.cleanup(self.scopes)
		self.emit_line(exit_line)

	# Walk statements in order and require each one to follow a continuing path.
	def statements(self, nodes: tuple[Node, ...]) -> bool:
		continues = True
		# A terminating statement still lets the walker check for trailing statements.
		for node in nodes:
			# Point to the first unreachable statement after a terminator.
			if not continues:
				raise SourceError(node.line)
			continues = self.statement(node)
		return continues

	# Enter a scope and drop its locals when the block falls through.
	def block(self, node: Form, test: Node | None = None) -> bool:
		self.scopes.append({})
		# Lower the loop's condition inside the C loop so it runs each iteration.
		if test is not None:
			condition = self.typed(test, BOOL)
			self.emit_line(f"if (!{condition.c}) {{ break; }}")
		continues = self.statements(node.operands)
		# A return has already dropped the locals in this block.
		if continues:
			self.cleanup(self.scopes[-1:])
		scope = self.scopes.pop()
		self.moved.difference_update(scope.values())
		return continues

	# Emit a boolean branch and merge ownership on paths that reach its join.
	def conditional(self, node: Form) -> bool:
		# An if has a condition, a then arm, and at most one else arm.
		if len(node.operands) not in (2, 3):
			raise SourceError(node.line)
		test, then_node, *otherwise = node.operands
		then_arm = checked_form(then_node, "then")
		else_arm = checked_form(otherwise[0], "else") if otherwise else None
		condition = self.typed(test, BOOL)

		# Both arms start with the state after evaluating the condition.
		incoming = self.moved.copy()
		self.emit_line(f"if ({condition.c}) {{")
		then_continues = self.block(then_arm)
		then_moved = self.moved if then_continues else set()
		self.moved = incoming

		# An omitted else reaches the join with the incoming state.
		else_continues = True
		# Emit the explicit else in its own scope from the incoming state.
		if else_arm is not None:
			self.emit_line("} else {")
			else_continues = self.block(else_arm)
		self.emit_line("}")

		# Any continuing arm can make an outer owner unavailable at the join.
		self.moved = then_moved | self.moved if else_continues else then_moved
		return then_continues or else_continues

	# Emit a pre-test loop with one body scope per iteration.
	def loop(self, node: Form) -> None:
		test, body_node = form(node, arity=2).operands
		body = checked_form(body_node, "body")
		incoming = self.moved.copy()
		self.emit_line("while (true) {")
		continues = self.block(body, test)
		# Outer owners available at entry must survive every path back to the test.
		if continues and self.moved - incoming:
			raise SourceError(node.line)
		self.moved = incoming
		self.emit_line("}")

	# Lower a statement and report whether execution can continue past it.
	def statement(self, node: Node) -> bool:
		statement = node = checked_form(node)
		# Branches emit and join their own lexical scopes.
		if statement.tag == "if":
			return self.conditional(statement)

		# A loop may continue past its body when the condition becomes false.
		if statement.tag == "while":
			self.loop(statement)
			return True

		# Assignment updates the named scalar's own C slot.
		if statement.tag == "set":
			target, operand = form(statement, arity=2).operands
			binding = self.local(target, node.line)
			# Only writable scalar locals have assignable slots.
			if not binding.writable or binding.value.kind not in SCALAR_TYPES:
				raise SourceError(node.line)
			value = self.typed(operand, binding.value.kind)
			self.emit_line(f"{binding.value.c} = {value.c};")
			return True

		# A return exits the function and every enclosing block.
		if statement.tag == "return":
			self.emit_return(statement)
			return False

		# Store directly into the backing storage of a mutable owner or view.
		if statement.tag == "array.store":
			target, subscript, operand = form(statement, arity=3).operands
			binding, kind, fields = self.array_access(target, node.line, writable=True)
			element = kind.element
			checkpoint = len(self.loans)
			# Projecting a writable view of the owner reserves it through both operands.
			if kind.access == OWNED:
				self.loans.append((binding, MUT))
			# Restore the enclosing loan prefix if either operand fails checking.
			try:
				index = self.typed(subscript, I32)
				value = self.typed(operand, element)
			finally:
				del self.loans[checkpoint:]
			# Keep the byte path for existing callers and use native assignment for records.
			if element == U8:
				self.emit_line(
					f"wvrt_slice_store_u8({fields}wvrt_items, {fields}wvrt_length, "
					f"{index.c}, {value.c}, {self.site(node.line)});"
				)
			# The checked element index selects one writable record slot.
			else:
				slot = self.record_element(fields, element, index, node.line, writable=True)
				self.emit_line(f"{slot} = {value.c};")
			return True

		# Calls used as statements complete without a result.
		if statement.tag == "call":
			self.typed(statement, VOID)
			return True

		# Every other statement in the seed introduces a binding.
		if statement.tag not in ("let", "let-mut"):
			raise SourceError(node.line)
		name, initializer = form(statement, arity=2).operands
		symbol = identifier(name)
		scope = self.scopes[-1]
		# Each name can be declared once in the current lexical scope.
		if symbol in scope:
			raise SourceError(name.line)
		value = self.expression(initializer)
		# A binding needs a value to own or copy.
		if value.kind == VOID:
			raise SourceError(initializer.line)
		writable = statement.tag == "let-mut"
		# A mutable copyable binding needs its own slot.
		if writable and copyable(value.kind):
			value = self.temporary(value.kind, value.c)
		scope[symbol] = Binding(value, writable=writable)
		return True

	# Emit the function and require a result from each value-returning exit path.
	def emit(self, retained: tuple[str, ...] = ()) -> list[str]:
		body = self.function.body
		self.lines.append(signature(self.function) + " {")
		# Every named parameter may be unused in the Wove body.
		for index in range(len(self.function.params)):
			self.emit_line(f"(void)wv_value_{index};")
		# Keep loaded provider functions and module bindings private while marking them used.
		for name in retained:
			self.emit_line(f"(void){name};")
		# A path reaching the end of the body may only exit a void function.
		if self.statements(body.operands):
			# A result must be supplied by an explicit return.
			if self.function.result != VOID:
				raise SourceError(body.line)
			self.cleanup(self.scopes)
		self.lines.extend(["}", ""])
		return self.lines


# Stage 1 selects bootstrap with test mode disabled.
def cfg_matches(node: Node) -> bool:
	negated = isinstance(node, Form)
	# The seed only supports negating a single named predicate.
	if negated:
		node, = form(node, "not", 1).operands
	predicate = identifier(node)
	# An unsupported predicate cannot silently select a different declaration.
	if predicate not in ("bootstrap", "test"):
		raise SourceError(node.line)
	return (predicate == "bootstrap") != negated


# Apply the seed's known selection pragmas to an eligible form.
def metadata_enabled(node: Form, *, selectable: bool = False) -> bool:
	enabled = True
	# Other metadata entries and unknown pragma payloads stay opaque to Stage 1.
	for entry in node.metadata:
		# Only pragma entries affect selection.
		if entry.tag != ":pragma":
			continue
		# Validate every known directive, including those after an exclusion.
		for directive in entry.operands:
			pragma = form(directive)
			# Each cfg must permit the bootstrap build.
			if pragma.tag == "cfg":
				predicate, = form(pragma, arity=1).operands
				selected = cfg_matches(predicate)
			# Test declarations are excluded from the bootstrap build.
			elif pragma.tag == "test":
				form(pragma, arity=0)
				selected = False
			# Leave the payload of an unknown directive uninterpreted.
			else:
				continue
			# Module roots, declarations, and declaration groups support selection.
			if not selectable:
				raise SourceError(pragma.line)
			enabled = selected and enabled
	return enabled


# Validate metadata on an ordinary operand or statement.
def checked_form(node: Node, tag: str | None = None, arity: int | None = None) -> Form:
	value = form(node, tag, arity)
	metadata_enabled(value)
	return value


# Classify a use by its bare provider while leaving every other operand opaque.
def use_provider(node: Form) -> str | None:
	# An absent, quoted, or malformed provider goes through ordinary use diagnostics.
	if node.operands:
		provider = node.operands[0]
		# Keep an arbitrary well-formed foreign provider available to the skip rule.
		if isinstance(provider, Atom) and not provider.quoted and IDENTIFIER.fullmatch(provider.text):
			return provider.text
	return None


# Flatten active groups in source order before declaring any names.
def selected_declarations(nodes: tuple[Node, ...], *, skip_host: bool = False) -> Iterator[Form]:
	# Exclude declarations before opening imported files or validating bodies.
	for node in nodes:
		declaration = form(node)
		# Bootstrap discards skipped host providers before interpreting their pragmas.
		if skip_host:
			# A bridge and every member belong to later compilation stages.
			if declaration.tag == "bridge":
				continue
			# Wove edges and C-header includes both participate in bootstrap selection.
			if declaration.tag == "use" and use_provider(declaration) not in (None, "wove-file", "c-header"):
				continue
		# An inactive declaration or group contributes no definitions.
		if not metadata_enabled(declaration, selectable=True):
			continue
		# A group declares its active children in the enclosing module.
		if declaration.tag == "decls":
			yield from selected_declarations(declaration.operands, skip_host=skip_host)
		# Keep active declarations and their source locations intact.
		else:
			yield declaration


# Add the defining file to any line-only error from local parsing or lowering.
@contextmanager
def source_file(filename: str) -> Iterator[None]:
	# Keep a nested import's diagnostic at its original file.
	try:
		yield
	except SourceError as error:
		# Most syntax and type checks only know the local line.
		if error.filename is None:
			error.filename = filename
		raise


# Collect active local declarations and preserve their emission order.
def read_module(
	source: str, filename: str, record_index: int, function_index: int, value_index: int, *, entry: bool = False
) -> Module:
	# Parsing and declaration selection belong to the defining file.
	with source_file(filename):
		module = form(parse(source), "module")
		# The module name precedes its declarations.
		if not module.operands:
			raise SourceError(module.line)
		identifier(module.operands[0])
		# An excluded file contributes no declarations or edges to the import graph.
		if not metadata_enabled(module, selectable=True):
			# A build still needs its entry module to be active.
			if entry:
				raise SourceError(module.line)
			return Module(filename, function_index)
		records = {}
		function_declarations = {}
		uses = []
		values = {}
		# Select active declarations before looking at any import's operands or path.
		for node in selected_declarations(module.operands[1:], skip_host=True):
			# Collect imports for the dependency walk.
			if node.tag == "use":
				uses.append(node)
			# Local record layouts contain only scalars, so imports cannot affect them.
			elif node.tag == "struct":
				name, record = read_record(node, record_index + len(records))
				# A file has one declaration per record name.
				if name in records:
					raise SourceError(node.line)
				records[name] = record
			# Seed module storage only accepts immutable literal declarations.
			elif node.tag == "let":
				name, value = read_module_value(node, value_index + len(values))
				# A file has one module binding per value name.
				if name in values:
					raise SourceError(node.line)
				values[name] = value
			# Collect function names now and check their signatures after imports bind.
			else:
				name = identifier(form(node, "fn").operands[0])
				# A file has one declaration per function name in its function namespace.
				if name in function_declarations:
					raise SourceError(node.line)
				function_declarations[name] = node
		# The root supplies the executable entry point even when a provider defines main.
		if entry and "main" not in function_declarations:
			raise SourceError(module.line)
		return Module(filename, function_index, records, function_declarations, uses, records.copy(), values=values)


# Resolve signatures in the file's complete type namespace before binding callers.
def prepare_functions(module: Module, *, entry: bool) -> None:
	# Report any invalid signature in its own defining file.
	with source_file(module.filename):
		# Number only local definitions, in their original declaration order.
		for index, (name, node) in enumerate(module.function_declarations.items(), module.function_index):
			module.functions[name] = read_function(name, node, index, module.type_bindings, entry_module=entry)
	module.bindings.update(module.functions)
	module.value_bindings.update(module.values)


# Decode the seed's local-file provider and its explicit selection list.
def read_use(node: Form) -> tuple[str, Form]:
	provider, source, only = form(node, "use", 3).operands
	# Stage 1 only opens Wove source files.
	if identifier(provider) != "wove-file":
		raise SourceError(provider.line)
	# The path is a quoted string, including when it contains escaped characters.
	if not isinstance(source, Atom) or not source.quoted:
		raise SourceError(source.line)
	return source.text, checked_form(only, "only")


# Derive the deduplication key and emitted operand from an active C-header use.
def read_header_include(node: Form, directory: Path) -> tuple[str, str]:
	# A C-header provider needs at least its header operand.
	if len(node.operands) < 2:
		raise SourceError(node.line)
	operand = node.operands[1]
	# The outer Wove quotes protect the C search delimiters and path.
	if not isinstance(operand, Atom) or not operand.quoted:
		raise SourceError(operand.line)
	spelling = operand.text
	opener = spelling[:1]
	# Validate the complete literal before emitting an include directive.
	if opener in ('<', '"'):
		closer = ">" if opener == "<" else '"'
		# Only a matched delimiter pair can enclose a nonempty path.
		if not spelling.endswith(closer):
			raise SourceError(operand.line)
		spelling = spelling[1:-1]
	# Match the native include operand's path alphabet.
	if not HEADER_PATH.fullmatch(spelling):
		raise SourceError(operand.line)
	# Angle brackets keep system search with the original checked path.
	if opener == "<":
		include = f"<{spelling}>"
		return include, include

	candidate = directory / spelling
	key = str(candidate)
	anchored = spelling.startswith(("./", "../", "/"))
	# Local files use their resolved containing directory for comparison.
	try:
		# Keep the header basename so file aliases can retain their include context.
		if candidate.is_file():
			anchored = True
			key = os.path.join(os.path.realpath(candidate.parent, strict=True), candidate.name)
	# If the path cannot be inspected, keep the original lookup for Clang.
	except OSError:
		pass
	# Anchor local paths without collapsing parent traversal across symlinks.
	if anchored:
		spelling = str(candidate)
	else:
		key = spelling
	# The containing directory can contain spaces but must fit a C header-name token.
	if any(byte in spelling for byte in '"\r\n\0'):
		raise SourceError(operand.line)
	return f'"{key}"', f'"{spelling}"'


# Bind provider declarations directly so aliases retain the defining identity.
def bind_use(only: Form, provider: Module, importer: Module) -> None:
	# Each item selects a definition in its provider, including an immutable value.
	for item in only.operands:
		namespace = item.tag if isinstance(item, Form) and item.tag in ("type", "value") else None
		# Qualified selectors share the same bare-name and alias syntax.
		if namespace is not None:
			item, = checked_form(item, namespace, 1).operands
		# A bare name uses the provider spelling locally.
		if isinstance(item, Atom):
			original = local = item
		# A renamed import carries both the defining and local names.
		else:
			original, local = checked_form(item, "as", 2).operands
		spelling = identifier(original)
		# An explicit type selector always uses the provider's own type definitions.
		if namespace == "type":
			target = provider.records.get(spelling)
			bindings = importer.type_bindings
			reserved = C_TYPE
		# An unqualified selection preserves function precedence at the provider.
		elif namespace is None and spelling in provider.functions:
			target = provider.functions[spelling]
			bindings = importer.bindings
			reserved = importer.function_declarations
		# Value bindings share the selected declaration's existing storage.
		else:
			target = provider.values.get(spelling)
			bindings = importer.value_bindings
			reserved = importer.values
		# Only declarations defined in the provider can be selected.
		if target is None:
			raise SourceError(original.line)
		name = identifier(local)
		# Keep builtins and local declarations distinct from imports in their own namespace.
		if name in bindings or name in reserved:
			raise SourceError(local.line)
		bindings[name] = target


# Read an active provider and distinguish its source errors from failures to open it.
def read_provider(path: Path, filename: str, use: Form) -> str:
	# File and encoding failures have different source origins.
	try:
		return path.read_text(encoding="utf-8")
	except OSError:
		raise SourceError(use.line) from None
	except UnicodeDecodeError as error:
		prefix = error.object[:error.start]
		line = prefix.count(b"\n") + prefix.count(b"\r") - prefix.count(b"\r\n") + 1
		raise SourceError(line, filename) from None


# Identify files across spelling aliases, symbolic links, and hard links.
def file_identity(path: Path) -> tuple[int, int]:
	status = path.stat()
	return status.st_dev, status.st_ino


# Visit the active source graph and collect includes at their first use.
def load_modules(root: Module) -> tuple[list[Module], list[str]]:
	# A file with no uses only needs signature preparation.
	if not root.uses:
		prepare_functions(root, entry=True)
		return [root], [RUNTIME_INCLUDE]
	path = Path(root.filename).resolve()
	# The root text can also be supplied in memory under a prospective filename.
	try:
		root_identity = file_identity(path)
	except FileNotFoundError:
		root_identity = path
	modules = {root_identity: root}
	headers = {RUNTIME_INCLUDE: RUNTIME_INCLUDE}
	active = {root_identity}
	stack = [(root_identity, path, iter(root.uses), None)]
	record_index = len(root.records)
	function_index = len(root.function_declarations)
	value_index = len(root.values)
	absolute_paths = Path(root.filename).is_absolute()
	# Keep active importers on the stack until their last use has been processed.
	while stack:
		identity, path, uses, incoming = stack[-1]
		importer = modules[identity]
		use = next(uses, None)
		# A file's signatures are ready when all of its imports have bound.
		if use is None:
			prepare_functions(importer, entry=incoming is None)
			active.remove(identity)
			stack.pop()
			# Bind the pending edge that brought this file into the graph.
			if incoming is not None:
				parent, only = incoming
				# Invalid selections belong to the importer.
				with source_file(parent.filename):
					bind_use(only, importer, parent)
			continue
		# An include, edge, or invalid selection is diagnosed in this importer.
		with source_file(importer.filename):
			# An explicit C header contributes no Wove names or dependency edges.
			if use_provider(use) == "c-header":
				key, include = read_header_include(use, path.parent)
				headers.setdefault(key, include)
				continue
			requested, only = read_use(use)
			# Resolve from the file containing the use and collapse path aliases.
			try:
				target = (path.parent / requested).resolve(strict=True)
				target_identity = file_identity(target)
			except (OSError, RuntimeError, ValueError):
				raise SourceError(use.line) from None
			# An edge back into the current path is a cycle.
			if target_identity in active:
				raise SourceError(use.line)
			provider = modules.get(target_identity)
			# A provider reached again already has its complete signatures.
			if provider is not None:
				bind_use(only, provider, importer)
				continue
			filename = str(target) if absolute_paths else os.path.relpath(target)
			source = read_provider(target, filename, use)
			provider = read_module(source, filename, record_index, function_index, value_index)
			modules[target_identity] = provider
			record_index += len(provider.records)
			function_index += len(provider.function_declarations)
			value_index += len(provider.values)
			active.add(target_identity)
			stack.append((target_identity, target, iter(provider.uses), (importer, only)))
	return list(modules.values()), list(headers.values())


# Emit a complete C translation unit after the source graph succeeds.
def compile_source(source: str, filename: str) -> str:
	root = read_module(source, filename, 0, 0, 0, entry=True)
	modules, headers = load_modules(root)
	functions = [function for module in modules for function in module.functions.values()]
	values = [value for module in modules for value in module.values.values()]
	retained = (
		*(function.c_name for module in modules[1:] for function in module.functions.values()),
		*(value.binding.value.c for value in values),
	)

	lines = [f"#include {operand}" for operand in headers]
	lines.append("")
	# Define every native type before functions use it by value.
	for module in modules:
		# Each record is emitted only for its defining file.
		for record in module.records.values():
			lines.extend(record_definition(record))
	# Emit each module binding once, ahead of every function body.
	for value in values:
		lines.extend(module_value_definition(value))
	# Separate static storage from function declarations when the file has bindings.
	if values:
		lines.append("")
	lines.extend(signature(function) + ";" for function in functions)
	lines.append("")
	# Resolve names and trap locations in the function's defining file.
	for module in modules:
		path = c_string(module.filename)
		# Lower each body against its file's local types and selected function bindings.
		with source_file(module.filename):
			# Emit provider bodies only once, regardless of their local aliases.
			for function in module.functions.values():
				references = retained if function.is_entry else ()
				lines.extend(FunctionEmitter(function, module, path).emit(references))
				# Reuse the runtime's bounded host argv adapter for the two-view entry.
				if function.host_span is not None:
					lines.extend([f"WVRT_HOST_ENTRY({function.c_name}, {function.host_span.c_name})", ""])
	return "\n".join(lines)

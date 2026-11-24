# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Lower the first Wove ownership forms to C11."""

from collections.abc import Iterator
from contextlib import contextmanager
from dataclasses import dataclass
import os
from pathlib import Path
import re

from syntax import Atom, Form, Node, SourceError, form, identifier, parse


I32 = "i32"
U8 = "u8"
U32 = "u32"
BOOL = "bool"
VOID = "void"
SCALAR_LIMITS = {
	I32: (-(2**31), 2**31 - 1),
	U8: (0, 255),
	U32: (0, 2**32 - 1),
}
SCALAR_TYPES = (*SCALAR_LIMITS, BOOL)
OWNED = "owned-array"
SHARED = "shared-slice"
MUT = "mut-slice"
C_TYPE = {
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
	"i32.add.wrapping": (I32, "wvrt_i32_add_wrapping"),
	"i32.sub.wrapping": (I32, "wvrt_i32_sub_wrapping"),
	"u32.add.wrapping": (U32, "wvrt_u32_add_wrapping"),
	"u32.sub.wrapping": (U32, "wvrt_u32_sub_wrapping"),
}
COMPARISON_OPS = {
	"u8.eq": (U8, "=="),
	"u8.lt": (U8, "<"),
	"i32.eq": (I32, "=="),
	"i32.lt": (I32, "<"),
	"u32.eq": (U32, "=="),
	"u32.lt": (U32, "<"),
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


@dataclass(frozen=True)
class Value:
	kind: Type
	c: str


@dataclass(eq=False)
class Binding:
	value: Value
	writable: bool = False


@dataclass(frozen=True)
class Function:
	c_name: str
	params: tuple[tuple[str, Type], ...]
	result: str | Record
	body: Form


@dataclass(frozen=True)
class Module:
	filename: str
	records: dict[str, Record]
	functions: dict[str, Function]
	uses: list[Form]
	bindings: dict[str, Function]


# Scalars and scalar-only records can be passed without a loan or transfer.
def copyable(kind: Type) -> bool:
	return isinstance(kind, Record) or kind in SCALAR_TYPES


# Arrays and slices share descriptor layouts while values retain their native types.
def c_type(kind: Type) -> str:
	# The descriptor depends on the access mode and carries a typed element count.
	if isinstance(kind, BufferType):
		return C_TYPE[kind.access]
	return kind.c_name if isinstance(kind, Record) else C_TYPE[kind]


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


# Read the signature before lowering calls, including forward calls.
def read_function(
	node: Node, index: int, records: dict[str, Record], *, entry_module: bool
) -> tuple[str, Function]:
	name_node, params_node, returns_node, body_node = form(node, "fn", 4).operands
	name = identifier(name_node)

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
	# Keep the root's C entry point on its existing zero-argument i32 ABI.
	if entry and (params or result != I32):
		raise SourceError(node.line)

	c_name = "main" if entry else f"wv_seed_fn_{index}"
	return name, Function(c_name, tuple(params.items()), result, checked_form(body_node, "body"))


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
	result = "int" if function.c_name == "main" else f"static {c_type(function.result)}"
	return f"{result} {function.c_name}({params or 'void'})"


class FunctionEmitter:
	# Initialize the function scope, C names, and ownership state.
	def __init__(
		self, function: Function, functions: dict[str, Function], records: dict[str, Record], source: str
	):
		self.function = function
		self.functions = functions
		self.records = records
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
		raise SourceError(line)

	# Require the exact result type at the operation that consumes it.
	def typed(self, node: Node, kind: Type) -> Value:
		value = self.expression(node)
		# Scalar arithmetic and calls need an explicit conversion between types.
		if value.kind != kind:
			raise SourceError(node.line)
		return value

	# Check an access against loans held by earlier arguments of active calls.
	def check_access(self, binding: Binding, exclusive: bool, line: int) -> None:
		# Shared access can coexist with other shared access to the same place.
		for parent, loan in self.loans:
			# Any exclusive access conflicts with an outstanding loan of that place.
			if parent is binding and (exclusive or loan == MUT):
				raise SourceError(line)

	# Resolve a slice and check its permission to access the parent storage.
	def slice(self, node: Node, line: int, writable: bool = False) -> tuple[Value, str | Record]:
		binding = self.local(node, line)
		value = binding.value
		kind = value.kind
		# Slice operations need a view, and stores require its mutable type.
		if not isinstance(kind, BufferType) or kind.access == OWNED or (writable and kind.access != MUT):
			raise SourceError(line)
		self.check_access(binding, writable, line)
		return value, kind.element

	# Index a native record only after the runtime verifies the signed element index.
	def record_element(
		self, view: Value, element: str | Record, index: Value, line: int, *, writable: bool = False
	) -> str:
		native = c_type(element)
		pointer = native if writable else f"const {native}"
		offset = f"wvrt_slice_offset({view.c}.wvrt_length, {index.c}, {self.site(line)})"
		return f"(({pointer} *){view.c}.wvrt_items)[{offset}]"

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
		# A range projects the named source for a shared call argument.
		if target.tag == "slice.range":
			# Range projection uses the runtime's shared byte-slice path.
			if exclusive or kind.element != U8:
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
			expression = (
				f"wvrt_slice_range_u8({fields}wvrt_items, {fields}wvrt_length, "
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
		callee = self.functions.get(identifier(name))
		# Resolve the signature before pairing arguments with parameters.
		if callee is None or callee.c_name == "main" or len(args) != len(callee.params):
			raise SourceError(node.line)
		checkpoint = len(self.loans)
		values = []
		# Each argument may acquire a loan that constrains later arguments.
		for argument, (_, kind) in zip(args, callee.params):
			value = self.borrow(argument, kind) if isinstance(kind, BufferType) else self.typed(argument, kind)
			values.append(value.c)
		expression = f"{callee.c_name}({', '.join(values)})"
		# A void call is a statement and supplies no C value to a consumer.
		if callee.result == VOID:
			self.emit_line(f"{expression};")
			result = Value(VOID, "")
		# Value-returning calls capture their result before the next operation.
		else:
			result = self.temporary(callee.result, expression)
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
		expr = node = checked_form(node)
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

		# Convert the decimal literal to a C-safe spelling.
		if expr.tag in SCALAR_LIMITS:
			literal, = form(expr, arity=1).operands
			# Restrict the spelling to a signed decimal atom.
			if not isinstance(literal, Atom) or literal.quoted or not re.fullmatch(r"-?[0-9]+", literal.text):
				raise SourceError(node.line)
			# Parse the value so C cannot interpret leading zeros as octal.
			try:
				number = int(literal.text)
			# Treat values beyond Python’s conversion limit as source failures.
			except ValueError:
				raise SourceError(node.line) from None
			low, high = SCALAR_LIMITS[expr.tag]
			# Check the declared type before handing the literal to C.
			if not low <= number <= high:
				raise SourceError(node.line)
			spelling = f"UINT32_C({number})" if expr.tag == U32 else str(number)
			return Value(expr.tag, spelling)

		# Check the selected type's range before casting the result to its C type.
		if expr.tag == "int.convert":
			target, operand = form(expr, arity=2).operands
			kind = identifier(target)
			# The target must name a seed integer type.
			if kind not in SCALAR_LIMITS:
				raise SourceError(node.line)
			value = self.expression(operand)
			# Conversion takes an integer value from any of the seed scalar types.
			if value.kind not in SCALAR_LIMITS:
				raise SourceError(node.line)
			low, high = SCALAR_LIMITS[kind]
			return self.temporary(
				kind, f"({C_TYPE[kind]})wvrt_int_convert({value.c}, {low}, {high}, "
				f"{self.site(node.line)})"
			)

		# Widen an unsigned byte so it can be used in an i32 expression or index.
		if expr.tag == "i32.from-u8":
			target, = form(expr, arity=1).operands
			value = self.typed(target, U8)
			return self.temporary(I32, f"(int32_t){value.c}")

		# A record construction produces a new value of its declared type.
		if expr.tag == "record.make":
			return self.make_record(expr)

		# A member access produces a copy of the declared scalar field.
		if expr.tag == "record.get":
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

		# Read the length from the non-owning slice descriptor.
		if expr.tag == "slice.len":
			target, = form(expr, arity=1).operands
			view, _ = self.slice(target, node.line)
			return self.temporary(I32, f"{view.c}.wvrt_length")

		# Copy an element through either descriptor after checking its index.
		if expr.tag == "slice.load":
			target, subscript = form(expr, arity=2).operands
			view, element = self.slice(target, node.line)
			index = self.typed(subscript, I32)
			# A typed record access copies the entire element.
			if element != U8:
				return self.temporary(element, self.record_element(view, element, index, node.line))
			return self.temporary(
				U8, f"wvrt_slice_load_u8({view.c}.wvrt_items, {view.c}.wvrt_length, "
				f"{index.c}, {self.site(node.line)})"
			)

		wrapping = WRAPPING_OPS.get(expr.tag)
		# Evaluate exact-type operands in order before performing wrapping arithmetic.
		if wrapping is not None:
			kind, helper = wrapping
			left, right = form(expr, arity=2).operands
			first = self.typed(left, kind)
			second = self.typed(right, kind)
			return self.temporary(kind, f"{helper}({first.c}, {second.c})")

		comparison = COMPARISON_OPS.get(expr.tag)
		# Check both operand types before using the corresponding C comparison.
		if comparison is not None:
			kind, operator = comparison
			left, right = form(expr, arity=2).operands
			first = self.typed(left, kind)
			second = self.typed(right, kind)
			return self.temporary(BOOL, f"{first.c} {operator} {second.c}")

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

		# Store directly into the mutable view's backing storage.
		if statement.tag == "slice.store":
			target, subscript, operand = form(statement, arity=3).operands
			view, element = self.slice(target, node.line, writable=True)
			index = self.typed(subscript, I32)
			value = self.typed(operand, element)
			# Keep the byte path for existing callers and use native assignment for records.
			if element == U8:
				self.emit_line(
					f"wvrt_slice_store_u8({view.c}.wvrt_items, {view.c}.wvrt_length, "
					f"{index.c}, {value.c}, {self.site(node.line)});"
				)
			# The checked element index selects one writable record slot.
			else:
				slot = self.record_element(view, element, index, node.line, writable=True)
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
		# Mark loaded provider functions as used while keeping their C linkage private.
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


# Stage 1 always selects the bootstrap implementation.
def cfg_matches(node: Node) -> bool:
	negated = isinstance(node, Form)
	# The only compound predicate in the seed is (not bootstrap).
	if negated:
		node, = form(node, "not", 1).operands
	# An unsupported predicate cannot silently select a different declaration.
	if identifier(node) != "bootstrap":
		raise SourceError(node.line)
	return not negated


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


# Flatten active groups in source order before declaring any names.
def selected_declarations(nodes: tuple[Node, ...]) -> Iterator[Form]:
	# Exclude declarations before opening imported files or validating bodies.
	for node in nodes:
		declaration = form(node)
		# An inactive declaration or group contributes no definitions.
		if not metadata_enabled(declaration, selectable=True):
			continue
		# A group declares its active children in the enclosing module.
		if declaration.tag == "decls":
			yield from selected_declarations(declaration.operands)
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


# Read active declarations and establish the file's own signatures before loading providers.
def read_module(
	source: str, filename: str, record_index: int, function_index: int, *, entry: bool = False
) -> Module:
	# Parsing, declaration selection, and signature checks belong to this file.
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
			return Module(filename, {}, {}, [], {})
		records = {}
		function_declarations = []
		uses = []
		# Select active declarations before looking at any import's operands or path.
		for node in selected_declarations(module.operands[1:]):
			# Providers are loaded after the file's own types and functions are known.
			if node.tag == "use":
				uses.append(node)
			# Read declared record types before validating function signatures.
			elif node.tag == "struct":
				name, record = read_record(node, record_index + len(records))
				# A file has one layout for each named record.
				if name in records:
					raise SourceError(node.line)
				records[name] = record
			# All other active declarations must be functions.
			else:
				function_declarations.append(node)

		functions = {}
		# Read every local signature before translating any function body.
		for index, node in enumerate(function_declarations, function_index):
			symbol, function = read_function(node, index, records, entry_module=entry)
			# A file has one declaration per function name.
			if symbol in functions:
				raise SourceError(node.line)
			functions[symbol] = function
		# The root supplies the executable entry point even when a provider defines main.
		if entry and "main" not in functions:
			raise SourceError(module.line)
		return Module(filename, records, functions, uses, functions.copy())


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


# Bind provider declarations directly, so aliases keep the original type and identity.
def bind_use(only: Form, provider: Module, importer: Module) -> None:
	# Each item contributes one function name to the importer's scope.
	for item in only.operands:
		# A bare name uses the provider spelling locally.
		if isinstance(item, Atom):
			original = local = item
		# A renamed import carries both the defining and local names.
		else:
			original, local = checked_form(item, "as", 2).operands
		function = provider.functions.get(identifier(original))
		# A provider exposes only its own declarations to explicit file imports.
		if function is None:
			raise SourceError(original.line)
		name = identifier(local)
		# Local declarations and earlier import bindings share the same function scope.
		if name in importer.bindings:
			raise SourceError(local.line)
		importer.bindings[name] = function


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


# Visit the active source graph in declaration order and keep the first defining path.
def load_modules(root: Module, path: Path) -> list[Module]:
	# The root text can also be supplied in memory under a prospective filename.
	try:
		root_identity = file_identity(path)
	except FileNotFoundError:
		root_identity = path
	modules = {root_identity: root}
	active = {root_identity}
	stack = [(root_identity, path, iter(root.uses))]
	record_index = len(root.records)
	function_index = len(root.functions)
	absolute_paths = Path(root.filename).is_absolute()
	# Keep active importers on the stack until their last use has been processed.
	while stack:
		identity, path, uses = stack[-1]
		use = next(uses, None)
		# Leaving a file makes later paths to it shared dependencies.
		if use is None:
			active.remove(identity)
			stack.pop()
			continue
		importer = modules[identity]
		# An active edge or an invalid selection is diagnosed in this importer.
		with source_file(importer.filename):
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
			# Read and declare each active provider once, even with an empty selection.
			if provider is None:
				filename = str(target) if absolute_paths else os.path.relpath(target)
				source = read_provider(target, filename, use)
				provider = read_module(source, filename, record_index, function_index)
				modules[target_identity] = provider
				record_index += len(provider.records)
				function_index += len(provider.functions)
				active.add(target_identity)
				stack.append((target_identity, target, iter(provider.uses)))
			bind_use(only, provider, importer)
	return list(modules.values())


# Emit a complete C translation unit after the source graph succeeds.
def compile_source(source: str, filename: str) -> str:
	root = read_module(source, filename, 0, 0, entry=True)
	modules = load_modules(root, Path(filename).resolve()) if root.uses else [root]
	functions = [function for module in modules for function in module.functions.values()]
	retained = tuple(function.c_name for module in modules[1:] for function in module.functions.values())

	lines = ['#include "wove_runtime.h"', ""]
	# Define every native type before functions use it by value.
	for module in modules:
		# Each record is emitted only for its defining file.
		for record in module.records.values():
			lines.extend(record_definition(record))
	lines.extend(signature(function) + ";" for function in functions)
	lines.append("")
	# Resolve names and trap locations in the function's defining file.
	for module in modules:
		path = c_string(module.filename)
		# Lower each body against its file's local types and selected function bindings.
		with source_file(module.filename):
			# Emit provider bodies only once, regardless of their local aliases.
			for function in module.functions.values():
				references = retained if function.c_name == "main" else ()
				lines.extend(FunctionEmitter(function, module.bindings, module.records, path).emit(references))
	return "\n".join(lines)

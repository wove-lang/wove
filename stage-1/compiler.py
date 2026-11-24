# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Lower the first Wove ownership forms to C11."""

from collections.abc import Iterator
from dataclasses import dataclass
import os
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
class Value:
	kind: str
	c: str


@dataclass(eq=False)
class Binding:
	value: Value
	writable: bool = False


@dataclass(frozen=True)
class Function:
	c_name: str
	params: tuple[tuple[str, str], ...]
	result: str
	body: Form


@dataclass(frozen=True)
class RecordField:
	kind: str
	c_name: str


@dataclass(frozen=True)
class Record:
	c_name: str
	fields: dict[str, RecordField]


# Scalars and scalar-only records can be passed without a loan or transfer.
def copyable(kind: str, records: dict[str, Record]) -> bool:
	return kind in SCALAR_TYPES or kind in records


# Use the generated typedef for a record without changing its source type.
def c_type(kind: str, records: dict[str, Record]) -> str:
	record = records.get(kind)
	return record.c_name if record is not None else C_TYPE[kind]


# Collect scalar fields in declaration order for the record's native layout.
def read_record(node: Form, index: int) -> tuple[str, Record]:
	name_node, fields_node = form(node, "struct", 2).operands
	name = identifier(name_node)
	# Keep named records distinct from builtin value and loan types.
	if name in C_TYPE:
		raise SourceError(name_node.line)
	fields = {}
	# Only scalar fields participate in the seed's copyable record layout.
	for field_index, field_node in enumerate(target_form(fields_node, "fields").operands):
		field = target_form(field_node, arity=1)
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


# Read a copyable input or one of the two byte slice parameter types.
def parameter_type(node: Node, records: dict[str, Record]) -> str:
	# Copyable types are written as bare symbols.
	if isinstance(node, Atom) and not node.quoted and copyable(node.text, records):
		return node.text
	view = target_form(node, arity=1)
	# Slice parameters specify their loan kind and byte element type.
	if view.tag not in (SHARED, MUT) or identifier(view.operands[0]) != "u8":
		raise SourceError(node.line)
	return view.tag


# Read the signature before lowering calls, including forward calls.
def read_function(node: Node, index: int, records: dict[str, Record]) -> tuple[str, Function]:
	name_node, params_node, returns_node, body_node = form(node, "fn", 4).operands
	name = identifier(name_node)

	params = {}
	# Preserve parameter order for the generated C signature.
	for param in target_form(params_node, "params").operands:
		field = target_form(param, arity=1)
		# Each ordinary parameter name introduces a distinct binding.
		if field.tag.startswith(":") or field.tag in params:
			raise SourceError(param.line)
		params[field.tag] = parameter_type(field.operands[0], records)
	result_node, = target_form(returns_node, "returns", 1).operands
	result = identifier(result_node)
	# A function returns a copyable value or completes without a value.
	if result != VOID and not copyable(result, records):
		raise SourceError(result_node.line)
	# Keep the C entry point on its existing zero-argument i32 ABI.
	if name == "main" and (params or result != I32):
		raise SourceError(node.line)

	c_name = "main" if name == "main" else f"wv_seed_fn_{index}"
	return name, Function(c_name, tuple(params.items()), result, target_form(body_node, "body"))


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
def signature(function: Function, records: dict[str, Record]) -> str:
	params = ", ".join(
		f"{c_type(kind, records)} wv_value_{index}"
		for index, (_, kind) in enumerate(function.params)
	)
	result = "int" if function.c_name == "main" else f"static {c_type(function.result, records)}"
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
	def temporary(self, kind: str, expression: str) -> Value:
		name = f"wv_value_{self.next_value}"
		self.next_value += 1
		self.emit_line(f"{c_type(kind, self.records)} {name} = {expression};")
		return Value(kind, name)

	# Resolve a live place and report failures at the operation using it.
	def local(self, node: Node, line: int) -> Binding:
		name, = target_form(node, "local", 1).operands
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
	def typed(self, node: Node, kind: str) -> Value:
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

	# Resolve a byte slice and check its permission to access the parent storage.
	def slice(self, node: Node, line: int, writable: bool = False) -> Value:
		binding = self.local(node, line)
		value = binding.value
		# Slice operations need a view, and stores require its mutable type.
		if value.kind not in (SHARED, MUT) or (writable and value.kind != MUT):
			raise SourceError(line)
		self.check_access(binding, writable, line)
		return value

	# Supply the source line to runtime checks emitted for this operation.
	def site(self, line: int) -> str:
		return f"(wvrt_location){{{self.source}, {line}, 1}}"

	# Hold this loan through the remaining argument evaluations and the call.
	def borrow(self, node: Node, kind: str) -> Value:
		tag = "borrow-mut" if kind == MUT else "borrow-shared"
		target_node, = target_form(node, tag, 1).operands
		target = target_form(target_node)
		source = target
		range_bounds = None
		exclusive = kind == MUT
		# A range projects the named source for a shared call argument.
		if target.tag == "slice.range":
			# The seed supports shared range loans.
			if exclusive:
				raise SourceError(node.line)
			source, start, end = form(target, arity=3).operands
			range_bounds = (start, end)
		binding = self.local(source, node.line)
		origin = binding.value
		writable = binding.writable if origin.kind == OWNED else origin.kind == MUT
		# Exclusive access comes from a writable owner or a mutable slice type.
		if origin.kind not in (OWNED, SHARED, MUT) or (exclusive and not writable):
			raise SourceError(node.line)
		self.check_access(binding, exclusive, node.line)
		fields = origin.c + ("->" if origin.kind == OWNED else ".")

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
				f"({C_TYPE[kind]}){{{fields}wvrt_items, "
				f"{fields}wvrt_length, {fields}wvrt_element_size}}"
			)
		self.loans.append((binding, kind))
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
			value = self.borrow(argument, kind) if kind in (SHARED, MUT) else self.typed(argument, kind)
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
		kind = identifier(name)
		record = self.records.get(kind)
		# Only a declared record has a usable member layout.
		if record is None:
			raise SourceError(name.line)
		initializers = {}
		# Resolve each initializer to its C member before evaluating its value.
		for field_node in fields:
			initializer = target_form(field_node, arity=1)
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
		return self.temporary(kind, f"({record.c_name}){{{values}}}")

	# Read a scalar member from an evaluated record value.
	def record_field(self, node: Form) -> Value:
		target, name = form(node, arity=2).operands
		value = self.expression(target)
		record = self.records.get(value.kind)
		# A field lookup needs the target's declared record type.
		if record is None:
			raise SourceError(target.line)
		field = record.fields.get(identifier(name))
		# The declared type determines both the field's presence and scalar type.
		if field is None:
			raise SourceError(name.line)
		return self.temporary(field.kind, f"{value.c}.{field.c_name}")

	# Translate one operation, preserving ownership of every array value.
	def expression(self, node: Node) -> Value:
		expr = node = target_form(node)
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
			if not copyable(binding.value.kind, self.records):
				raise SourceError(node.line)
			# Capture the value now even if a later statement writes to its slot.
			if binding.writable:
				return self.temporary(binding.value.kind, binding.value.c)
			return binding.value

		# Allocate one runtime array with the source location for traps.
		if expr.tag == "array.alloc":
			element, length = form(expr, arity=2).operands
			# This seed uses the C uint8_t layout for every owned array.
			if identifier(element) != "u8":
				raise SourceError(node.line)
			size = self.typed(length, I32)
			site = self.site(node.line)
			return self.temporary(
				OWNED, f"wvrt_array_new({size.c}, sizeof(uint8_t), _Alignof(uint8_t), {site})"
			)

		# Transfer the pointer and invalidate the original owner slot.
		if expr.tag == "move":
			target, = form(expr, arity=1).operands
			binding = self.local(target, node.line)
			# Only an owner can transfer its allocation.
			if binding.value.kind != OWNED:
				raise SourceError(node.line)
			self.check_access(binding, True, node.line)
			result = self.temporary(OWNED, binding.value.c)
			self.moved.add(binding)
			self.emit_line(f"{binding.value.c} = NULL;")
			return result

		# Read the length from the non-owning slice descriptor.
		if expr.tag == "slice.len":
			target, = form(expr, arity=1).operands
			view = self.slice(target, node.line)
			return self.temporary(I32, f"{view.c}.wvrt_length")

		# Read a byte through either descriptor after checking its index.
		if expr.tag == "slice.load":
			target, subscript = form(expr, arity=2).operands
			view = self.slice(target, node.line)
			index = self.typed(subscript, I32)
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
				# A moved slot holds NULL on the path that moved it.
				if binding.value.kind == OWNED:
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
		then_arm = target_form(then_node, "then")
		else_arm = target_form(otherwise[0], "else") if otherwise else None
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
		body = target_form(body_node, "body")
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
		statement = node = target_form(node)
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
			view = self.slice(target, node.line, writable=True)
			index = self.typed(subscript, I32)
			value = self.typed(operand, U8)
			self.emit_line(
				f"wvrt_slice_store_u8({view.c}.wvrt_items, {view.c}.wvrt_length, "
				f"{index.c}, {value.c}, {self.site(node.line)});"
			)
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
		if writable and copyable(value.kind, self.records):
			value = self.temporary(value.kind, value.c)
		scope[symbol] = Binding(value, writable=writable)
		return True

	# Emit the function and require a result from each value-returning exit path.
	def emit(self) -> list[str]:
		body = self.function.body
		self.lines.append(signature(self.function, self.records) + " {")
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


# Read one wrapper and select its target only in a declaration context.
def metadata_target(wrapper: Form, *, declarations: bool = False) -> tuple[Form, bool]:
	meta, target = form(wrapper, "with-meta", 2).operands
	enabled = True
	# Validation here covers the entry tag and leaves other metadata payloads opaque.
	for node in form(meta, "meta").operands:
		entry = form(node)
		# The metadata list contains colon-prefixed forms.
		if not entry.tag.startswith(":"):
			raise SourceError(entry.line)
		# Only pragma payloads participate in Stage 1 selection.
		if entry.tag != ":pragma":
			continue
		# Check every known directive even when an earlier one excludes the target.
		for directive in entry.operands:
			pragma = form(directive)
			# Each cfg predicate must permit the bootstrap build.
			if pragma.tag == "cfg":
				predicate, = form(pragma, arity=1).operands
				selected = cfg_matches(predicate)
			# A test declaration is always excluded from the bootstrap build.
			elif pragma.tag == "test":
				form(pragma, arity=0)
				selected = False
			# Other directives are opaque to the seed.
			else:
				continue
			# Selection runs on module declarations and groups before name resolution.
			if not declarations:
				raise SourceError(pragma.line)
			enabled = selected and enabled
	return form(target), enabled


# Strip metadata from a value or statement before inspecting its ordinary shape.
def target_form(node: Node, tag: str | None = None, arity: int | None = None) -> Form:
	# Peel nested wrappers without opening a new lexical scope.
	while isinstance(node, Form) and node.tag == "with-meta":
		node, _ = metadata_target(node)
	return form(node, tag, arity)


# Flatten active groups in source order before declaring any names.
def selected_declarations(nodes: tuple[Node, ...]) -> Iterator[Form]:
	# Walk only the selected declaration tree so excluded bodies stay opaque.
	for node in nodes:
		declaration = form(node)
		# Only selected targets contribute declarations to the module.
		if declaration.tag == "with-meta":
			target, enabled = metadata_target(declaration, declarations=True)
			# The selected target may wrap another declaration or a group.
			if enabled:
				yield from selected_declarations((target,))
		# A selected group declares its items in the enclosing module.
		elif declaration.tag == "decls":
			yield from selected_declarations(declaration.operands)
		# Pass selected declarations to signature validation unchanged.
		else:
			yield declaration


# Emit a complete C translation unit after the entire module succeeds.
def compile_source(source: str, filename: str) -> str:
	module = target_form(parse(source), "module")
	# The module name precedes its declarations.
	if not module.operands:
		raise SourceError(module.line)
	identifier(module.operands[0])
	declarations = tuple(selected_declarations(module.operands[1:]))
	records = {}
	function_declarations = []
	# Collect all active record types before validating function signatures.
	for node in declarations:
		# The remaining declarations are checked as functions.
		if node.tag != "struct":
			function_declarations.append(node)
			continue
		name, record = read_record(node, len(records))
		# A module has one layout for each named record.
		if name in records:
			raise SourceError(node.line)
		records[name] = record

	functions = {}
	# Collect only selected signatures before translating any function body.
	for index, node in enumerate(function_declarations):
		symbol, function = read_function(node, index, records)
		# A top-level name identifies exactly one function.
		if symbol in functions:
			raise SourceError(node.line)
		functions[symbol] = function
	# A generated executable needs its C entry point.
	if "main" not in functions:
		raise SourceError(module.line)

	lines = ['#include "wove_runtime.h"', ""]
	# Define complete native layouts before functions use them by value.
	for record in records.values():
		lines.extend(record_definition(record))
	lines.extend(signature(fn, records) + ";" for fn in functions.values())
	lines.append("")
	path = c_string(filename)
	# Keep generated bodies in source order.
	for fn in functions.values():
		lines.extend(FunctionEmitter(fn, functions, records, path).emit())
	return "\n".join(lines)

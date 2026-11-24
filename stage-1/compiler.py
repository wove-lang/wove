# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Lower the first Wove ownership forms to C11."""

from dataclasses import dataclass
import os
import re

from syntax import Atom, Form, Node, SourceError, form, identifier, parse


I32 = "i32"
U8 = "u8"
U32 = "u32"
VOID = "void"
SCALAR_LIMITS = {
	I32: (-(2**31), 2**31 - 1),
	U8: (0, 255),
	U32: (0, 2**32 - 1),
}
OWNED = "owned-array"
SHARED = "shared-slice"
MUT = "mut-slice"
C_TYPE = {
	I32: "int32_t",
	U8: "uint8_t",
	U32: "uint32_t",
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


@dataclass(frozen=True)
class Value:
	kind: str
	c: str


@dataclass(eq=False)
class Binding:
	value: Value
	writable: bool = False
	moved: bool = False


@dataclass(frozen=True)
class Function:
	c_name: str
	params: tuple[tuple[str, str], ...]
	result: str
	expectations: tuple[Form, ...]
	body: Form


# Accept scalar inputs and the two byte slice parameter types.
def parameter_type(node: Node) -> str:
	# Scalar types are written as bare symbols.
	if isinstance(node, Atom) and not node.quoted and node.text in SCALAR_LIMITS:
		return node.text
	view = form(node, arity=1)
	# Slice parameters specify their loan kind and byte element type.
	if view.tag not in (SHARED, MUT) or identifier(view.operands[0]) != "u8":
		raise SourceError(node.line)
	return view.tag


# Keep expectation metadata on its function, including issue and fixture IDs.
def read_expectation(node: Node) -> Form:
	expectation = form(node, "expect")
	# The issue link is required and the fixture list is optional.
	if len(expectation.operands) not in (2, 3):
		raise SourceError(node.line)
	name, issue, *fixture_list = expectation.operands
	identifier(name)
	url, = form(issue, "issue", 1).operands
	# Keep the issue URL as a string in the expectation.
	if not isinstance(url, Atom) or not url.quoted:
		raise SourceError(url.line)
	# Preserve optional fixture IDs as function metadata.
	for tests in fixture_list:
		# Each fixture ID is a symbol in the IR.
		for test in form(tests, "tests").operands:
			identifier(test)
	return expectation


# Read the signature before lowering calls, including forward calls.
def read_function(node: Node, index: int) -> tuple[str, Function]:
	declaration = form(node, "fn")
	# Require the name, parameters, result, and body before unpacking.
	if len(declaration.operands) < 4:
		raise SourceError(node.line)
	name_node, params_node, returns_node, *metadata, body_node = declaration.operands
	name = identifier(name_node)

	params = {}
	# Preserve parameter order for the generated C signature.
	for param in form(params_node, "params").operands:
		field = form(param, arity=1)
		# Each parameter introduces a distinct binding.
		if field.tag in params:
			raise SourceError(param.line)
		params[field.tag] = parameter_type(field.operands[0])
	result_node, = form(returns_node, "returns", 1).operands
	result = identifier(result_node)
	# Seed functions return a scalar or complete without a value.
	if result != VOID and result not in SCALAR_LIMITS:
		raise SourceError(result_node.line)
	# Keep the C entry point on its existing zero-argument i32 ABI.
	if name == "main" and (params or result != I32):
		raise SourceError(node.line)

	c_name = "main" if name == "main" else f"wv_seed_fn_{index}"
	expectations = tuple(read_expectation(item) for item in metadata)
	return name, Function(c_name, tuple(params.items()), result, expectations, form(body_node, "body"))


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
		f"{C_TYPE[kind]} wv_value_{index}"
		for index, (_, kind) in enumerate(function.params)
	)
	result = "int" if function.c_name == "main" else f"static {C_TYPE[function.result]}"
	return f"{result} {function.c_name}({params or 'void'})"


class FunctionEmitter:
	# Initialize the bindings, temporaries, and outstanding loans for one function.
	def __init__(self, function: Function, functions: dict[str, Function], source: str):
		self.function = function
		self.functions = functions
		self.source = source
		self.lines = []
		self.bindings = {
			name: Binding(Value(kind, f"wv_value_{index}"))
			for index, (name, kind) in enumerate(function.params)
		}
		self.next_value = len(self.bindings)
		self.loans: list[tuple[Binding, str]] = []

	# Evaluate each operation in source order before emitting its consumer.
	def temporary(self, kind: str, expression: str) -> Value:
		name = f"wv_value_{self.next_value}"
		self.next_value += 1
		self.lines.append(f"    {C_TYPE[kind]} {name} = {expression};")
		return Value(kind, name)

	# Resolve a live place and report failures at the operation using it.
	def local(self, node: Node, line: int) -> Binding:
		name, = form(node, "local", 1).operands
		binding = self.bindings.get(identifier(name))
		# A missing or moved binding cannot supply a place.
		if binding is None or binding.moved:
			raise SourceError(line)
		return binding

	# Require the exact result type at the operation that consumes it.
	def typed(self, node: Node, kind: str) -> Value:
		value = self.expression(node)
		# Scalar arithmetic and calls need an explicit conversion between types.
		if value.kind != kind:
			raise SourceError(node.line)
		return value

	# Resolve a byte slice and grant stores only through an exclusive parameter.
	def slice(self, node: Node, line: int, writable: bool = False) -> Value:
		value = self.local(node, line).value
		# Both view types can read, but only mut-slice carries write permission.
		if value.kind not in (SHARED, MUT) or (writable and value.kind != MUT):
			raise SourceError(line)
		return value

	# Supply the source line to runtime checks emitted for this operation.
	def site(self, line: int) -> str:
		return f"(wvrt_location){{{self.source}, {line}, 1}}"

	# Hold this loan through the remaining argument evaluations and the call.
	def borrow(self, node: Node, kind: str) -> Value:
		tag = "borrow-mut" if kind == MUT else "borrow-shared"
		target, = form(node, tag, 1).operands
		binding = self.local(target, node.line)
		# An exclusive loan requires a writable owner.
		if binding.value.kind != OWNED or (kind == MUT and not binding.writable):
			raise SourceError(node.line)

		# Earlier argument loans remain active during nested calls.
		for owner, prior in self.loans:
			# Two loans of the same owner can coexist only when both are shared.
			if owner is binding and (kind == MUT or prior == MUT):
				raise SourceError(node.line)
		self.loans.append((binding, kind))
		owner = binding.value.c
		return self.temporary(
			kind, f"({C_TYPE[kind]}){{{owner}->wvrt_items, "
			f"{owner}->wvrt_length, {owner}->wvrt_element_size}}"
		)

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
			value = self.typed(argument, kind) if kind in SCALAR_LIMITS else self.borrow(argument, kind)
			values.append(value.c)
		expression = f"{callee.c_name}({', '.join(values)})"
		# A void call is a statement and supplies no C value to a consumer.
		if callee.result == VOID:
			self.lines.append(f"    {expression};")
			result = Value(VOID, "")
		# Scalar calls capture their result before the next operation.
		else:
			result = self.temporary(callee.result, expression)
		del self.loans[checkpoint:]
		return result

	# Translate one operation, preserving ownership of every array value.
	def expression(self, node: Node) -> Value:
		expr = form(node)
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

		# An ordinary value use copies a scalar binding.
		if expr.tag == "local":
			binding = self.local(expr, node.line)
			# Array and slice places need an explicit operation.
			if binding.value.kind not in SCALAR_LIMITS:
				raise SourceError(node.line)
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
			# A live loan keeps the original owner in place.
			if binding.value.kind != OWNED or any(owner is binding for owner, _ in self.loans):
				raise SourceError(node.line)
			result = self.temporary(OWNED, binding.value.c)
			binding.moved = True
			self.lines.append(f"    {binding.value.c} = NULL;")
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

		# Call lowering manages the lifetime of its argument loans.
		if expr.tag == "call":
			return self.call(expr)

		raise SourceError(node.line)

	# Lower statements that bind a value, call a void function, or store a byte.
	def statement(self, node: Node) -> None:
		statement = form(node)
		# Store directly into the mutable view's backing storage.
		if statement.tag == "slice.store":
			target, subscript, operand = form(statement, arity=3).operands
			view = self.slice(target, node.line, writable=True)
			index = self.typed(subscript, I32)
			value = self.typed(operand, U8)
			self.lines.append(
				f"    wvrt_slice_store_u8({view.c}.wvrt_items, {view.c}.wvrt_length, "
				f"{index.c}, {value.c}, {self.site(node.line)});"
			)
			return

		# Calls used as statements complete without a result.
		if statement.tag == "call":
			self.typed(statement, VOID)
			return

		# Every other statement in the seed introduces a binding.
		if statement.tag not in ("let", "let-mut"):
			raise SourceError(node.line)
		name, initializer = form(statement, arity=2).operands
		symbol = identifier(name)
		# A duplicate name would hide the original owner or parameter.
		if symbol in self.bindings:
			raise SourceError(name.line)
		value = self.expression(initializer)
		# A binding needs a value to own or copy.
		if value.kind == VOID:
			raise SourceError(initializer.line)
		self.bindings[symbol] = Binding(value, writable=statement.tag == "let-mut")

	# Emit a straight-line body and release its owners before leaving the function.
	def emit(self) -> list[str]:
		body = self.function.body
		statements = body.operands
		result = None
		# Scalar functions end in a return, while void functions fall through.
		if self.function.result != VOID:
			# The body must supply a return operand.
			if not statements:
				raise SourceError(body.line)
			*statements, last = statements
			result, = form(last, "return", 1).operands

		self.lines.append(signature(self.function) + " {")
		# Evaluate statements in source order while their locals remain in scope.
		for node in statements:
			self.statement(node)
		value = self.typed(result, self.function.result) if result is not None else None
		# Release owners in reverse declaration order after evaluating the result.
		for binding in reversed(self.bindings.values()):
			# The last owner is responsible for the release.
			if binding.value.kind == OWNED and not binding.moved:
				self.lines.append(f"    wvrt_array_drop({binding.value.c});")
		# A scalar result is ready to return after cleanup.
		if value is not None:
			self.lines.append(f"    return {value.c};")
		self.lines.extend(["}", ""])
		return self.lines


# Emit a complete C translation unit after the entire module succeeds.
def compile_source(source: str, filename: str) -> str:
	module = form(parse(source), "module")
	# The module name precedes its declarations.
	if not module.operands:
		raise SourceError(module.line)
	name, *declarations = module.operands
	identifier(name)
	functions = {}
	# Collect all signatures before translating any function body.
	for index, node in enumerate(declarations):
		symbol, function = read_function(node, index)
		# A top-level name identifies exactly one function.
		if symbol in functions:
			raise SourceError(node.line)
		functions[symbol] = function
	# A generated executable needs its C entry point.
	if "main" not in functions:
		raise SourceError(module.line)

	lines = ['#include "wove_runtime.h"', ""]
	lines.extend(signature(fn) + ";" for fn in functions.values())
	lines.append("")
	path = c_string(filename)
	# Keep generated bodies in source order.
	for fn in functions.values():
		lines.extend(FunctionEmitter(fn, functions, path).emit())
	return "\n".join(lines)

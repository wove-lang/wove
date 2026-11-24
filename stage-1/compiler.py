# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Lower the first Wove ownership forms to C11."""

from dataclasses import dataclass
import os
import re

from syntax import Atom, Form, Node, SourceError, form, identifier, parse


I32 = "i32"
OWNED = "owned-array"
SHARED = "shared-slice"
MUT = "mut-slice"
C_TYPE = {
	I32: "int32_t",
	OWNED: "wvrt_array *",
	SHARED: "wvrt_slice",
	MUT: "wvrt_mut_slice",
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
	expectations: tuple[Form, ...]
	body: Form


# Accept scalar inputs and the two byte slice parameter types.
def parameter_type(node: Node) -> str:
	# The scalar type is written as a bare symbol.
	if isinstance(node, Atom) and not node.quoted and node.text == I32:
		return I32
	view = form(node, arity=1)
	# Slice parameters specify their loan kind and byte element type.
	if view.tag not in (SHARED, MUT) or identifier(view.operands[0]) != "u8":
		raise SourceError(node.line)
	return view.tag


# Keep expectation metadata on its function, including issue and fixture IDs.
def read_expectation(node: Node) -> Form:
	expectation = form(node, "expect", 3)
	name, issue, tests = expectation.operands
	identifier(name)
	url, = form(issue, "issue", 1).operands
	# Keep the issue URL as a string in the expectation.
	if not isinstance(url, Atom) or not url.quoted:
		raise SourceError(url.line)
	# Read fixture IDs as symbols for the function metadata.
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
	result, = form(returns_node, "returns", 1).operands
	# The seed supports i32 returns and a zero-argument entry point.
	if identifier(result) != I32 or (name == "main" and params):
		raise SourceError(node.line)

	c_name = "main" if name == "main" else f"wv_seed_fn_{index}"
	expectations = tuple(read_expectation(item) for item in metadata)
	return name, Function(c_name, tuple(params.items()), expectations, form(body_node, "body"))


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
	result = "int" if function.c_name == "main" else "static int32_t"
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

	# Require a scalar in arithmetic, lengths, arguments, and returns.
	def i32(self, node: Node) -> Value:
		value = self.expression(node)
		# Only scalar values may flow through this expression position.
		if value.kind != I32:
			raise SourceError(node.line)
		return value

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
			value = self.i32(argument) if kind == I32 else self.borrow(argument, kind)
			values.append(value.c)
		result = self.temporary(I32, f"{callee.c_name}({', '.join(values)})")
		del self.loans[checkpoint:]
		return result

	# Translate one operation, preserving ownership of every array value.
	def expression(self, node: Node) -> Value:
		expr = form(node)
		# Convert the decimal literal to a C-safe spelling.
		if expr.tag == I32:
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
			# Enforce the signed i32 range before emitting C.
			if not -(2**31) <= number < 2**31:
				raise SourceError(node.line)
			return Value(I32, str(number))

		# An ordinary value use copies a scalar binding.
		if expr.tag == "local":
			binding = self.local(expr, node.line)
			# Array and slice places need an explicit operation.
			if binding.value.kind != I32:
				raise SourceError(node.line)
			return binding.value

		# Allocate one runtime array with the source location for traps.
		if expr.tag == "array.alloc":
			element, length = form(expr, arity=2).operands
			# This seed uses the C uint8_t layout for every owned array.
			if identifier(element) != "u8":
				raise SourceError(node.line)
			size = self.i32(length)
			site = f"(wvrt_location){{{self.source}, {node.line}, 1}}"
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
			binding = self.local(target, node.line)
			# Only slice descriptors support this operation.
			if binding.value.kind not in (SHARED, MUT):
				raise SourceError(node.line)
			return self.temporary(I32, f"{binding.value.c}.wvrt_length")

		# Evaluate operands in order before adding modulo 2^32.
		if expr.tag == "i32.add.wrapping":
			left, right = form(expr, arity=2).operands
			first = self.i32(left)
			second = self.i32(right)
			return self.temporary(I32, f"wvrt_i32_add_wrapping({first.c}, {second.c})")

		# Call lowering manages the lifetime of its argument loans.
		if expr.tag == "call":
			return self.call(expr)

		raise SourceError(node.line)

	# Emit straight-line bindings and release live owners at the return.
	def emit(self) -> list[str]:
		body = self.function.body
		# A straight-line function ends with an explicit return.
		if not body.operands:
			raise SourceError(body.line)
		self.lines.append(signature(self.function) + " {")

		*statements, last = body.operands
		# Introduce each binding after evaluating its initializer.
		for node in statements:
			statement = form(node, arity=2)
			# Only bindings may precede the return in this subset.
			if statement.tag not in ("let", "let-mut"):
				raise SourceError(node.line)
			name, initializer = statement.operands
			symbol = identifier(name)
			# A duplicate name would hide the original owner or parameter.
			if symbol in self.bindings:
				raise SourceError(name.line)
			value = self.expression(initializer)
			self.bindings[symbol] = Binding(value, writable=statement.tag == "let-mut")

		result, = form(last, "return", 1).operands
		value = self.i32(result)
		# Release owners in reverse declaration order after evaluating the result.
		for binding in reversed(self.bindings.values()):
			# The last owner is responsible for the release.
			if binding.value.kind == OWNED and not binding.moved:
				self.lines.append(f"    wvrt_array_drop({binding.value.c});")
		self.lines.extend([f"    return {value.c};", "}", ""])
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

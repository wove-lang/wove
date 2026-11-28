# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Read the C bridge declarations needed by the Python seed."""

from __future__ import annotations

from collections.abc import Iterator
from dataclasses import dataclass
import re

from syntax import IDENTIFIER, Atom, Form, Node, SourceError, form, identifier


C_TYPE_SPELLING = re.compile(r"[A-Za-z_][A-Za-z0-9_]*(?: +[A-Za-z_][A-Za-z0-9_]*)*(?: *\*)*")
# Match the byte pointer qualifications accepted by native C bridge arguments.
_C_READONLY_MEMBERS = (
	"const void *p0; const char *p1; const signed char *p2; const unsigned char *p3; "
	"const volatile void *p4; const volatile char *p5; "
	"const volatile signed char *p6; const volatile unsigned char *p7;"
)
_C_WRITABLE_MEMBERS = (
	"void *p8; char *p9; signed char *p10; unsigned char *p11; "
	"volatile void *p12; volatile char *p13; volatile signed char *p14; volatile unsigned char *p15;"
)
_C_BYTE_UNION = "union __attribute__((__transparent_union__))"
C_READONLY_BYTES = f"{_C_BYTE_UNION} {{{_C_READONLY_MEMBERS}}}"
C_WRITABLE_BYTES = f"{_C_BYTE_UNION} {{{_C_WRITABLE_MEMBERS}{_C_READONLY_MEMBERS}}}"


@dataclass(eq=False, frozen=True)
class NativeType:
	c_name: str
	owned: bool = False
	storage_drop: str | None = None


@dataclass(frozen=True)
class NativeDeclaration:
	node: Form
	enclosing: tuple[Form, ...]


# Visit declarations through groups without descending into disabled forms and bodies.
def declarations(nodes: tuple[Node, ...], enclosing: tuple[Form, ...]) -> Iterator[NativeDeclaration]:
	# Only groups contribute further declarations in the same namespace.
	for node in nodes:
		# Irrelevant bridge payloads are opaque to the seed.
		if not isinstance(node, Form):
			continue
		# Preserve selection pragmas on every group containing a declaration.
		if node.tag == "decls":
			yield from declarations(node.operands, (*enclosing, node))
		else:
			yield NativeDeclaration(node, enclosing)


# Match C bridges without opening other foreign providers.
def c_bridges(module: Form) -> Iterator[NativeDeclaration]:
	# Preserve the module's own selection when the bridge type is requested.
	for declaration in declarations(module.operands, (module,)):
		node = declaration.node
		# Other top-level declarations cannot provide a native type.
		if node.tag != "bridge" or len(node.operands) < 2:
			continue
		provider = node.operands[0]
		# The provider tag determines which declarations have C spellings.
		if isinstance(provider, Atom) and not provider.quoted and provider.text == "c-header":
			yield declaration


# Read the exact spelling and optional record fields of one selected native type.
def read_native_type(node: Form) -> tuple[str, dict[str, Node] | None]:
	slots = {child.tag: child for child in node.operands if isinstance(child, Form)}
	spelling = slots.get("c-type")
	# A native type needs an explicit spelling from its header.
	if spelling is None:
		raise SourceError(node.line)
	operand, = form(spelling, arity=1).operands
	# Keep the printed spelling inside ordinary C type tokens.
	if not isinstance(operand, Atom) or not operand.quoted or not C_TYPE_SPELLING.fullmatch(operand.text):
		raise SourceError(operand.line)
	# Opaque typedefs do not describe C record fields.
	if node.tag == "type":
		return operand.text, None
	fields = slots.get("fields")
	# An opaque record can still be used by value in generated C.
	if fields is None:
		return operand.text, {}
	return operand.text, {field.tag: form(field, arity=1).operands[0] for field in map(form, fields.operands)}


# Expose bridge pragmas without interpreting unrelated metadata.
def native_pragmas(node: Form) -> Iterator[Form]:
	return (form(operand) for entry in node.metadata if entry.tag == ":pragma" for operand in entry.operands)


# Read a fixed C function's signature and original symbol before resolving its types.
def read_native_function(node: Form) -> tuple[str, dict[str, Node], dict[str, str], Node]:
	fields = {}
	# A fixed signature has one parameter list, one result, and an optional C name.
	for operand in node.operands:
		field = form(operand)
		# Other native fields require their own lowering rules.
		if field.tag not in ("params", "returns", "c-name") or field.tag in fields:
			raise SourceError(field.line)
		fields[field.tag] = field
	# A bodyless function still supplies its complete signature.
	if "params" not in fields or "returns" not in fields:
		raise SourceError(node.line)
	parameters = {}
	constants = {}
	# Retain native formal order, including hidden header constants.
	for operand in fields["params"].operands:
		param = form(operand)
		# A fixed constant is part of the native signature but has no Wove argument.
		if param.tag == "const":
			name_node, kind, symbol = form(param, arity=3).operands
			name = identifier(name_node)
			constants[name] = identifier(symbol)
		else:
			kind, = form(param, arity=1).operands
			name = param.tag
		# Extent and variadic pragmas identify distinct named formals.
		if not IDENTIFIER.fullmatch(name) or name in parameters:
			raise SourceError(param.line)
		parameters[name] = kind
	result, = form(fields["returns"], arity=1).operands
	symbol = identifier(node.names[0])
	# A declaration may introduce a Wove alias for a differently named C function.
	if "c-name" in fields:
		spelling, = form(fields["c-name"], arity=1).operands
		# The quoted spelling must be one C identifier.
		if not isinstance(spelling, Atom) or not spelling.quoted or not IDENTIFIER.fullmatch(spelling.text):
			raise SourceError(spelling.line)
		symbol = spelling.text
	return symbol, parameters, constants, result

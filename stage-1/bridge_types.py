# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Find the native C type declarations the Python seed can lower."""

from __future__ import annotations

from collections.abc import Iterator
from dataclasses import dataclass
import re

from syntax import Atom, Form, Node, SourceError, form, identifier


C_TYPE_SPELLING = re.compile(r"[A-Za-z_][A-Za-z0-9_]*(?: +[A-Za-z_][A-Za-z0-9_]*)*(?: *\*)*")


@dataclass(eq=False, frozen=True)
class NativeType:
	c_name: str


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

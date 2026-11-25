# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Read tagged Wove IR forms and keep their source lines."""

from __future__ import annotations

from dataclasses import dataclass
import json
import re


class SourceError(Exception):
	# Keep the source line and, once known, its defining file available to the CLI.
	def __init__(self, line: int, filename: str | None = None):
		self.line = line
		self.filename = filename
		super().__init__(line)


@dataclass(frozen=True)
class Atom:
	text: str
	line: int
	quoted: bool = False


@dataclass(frozen=True)
class Form:
	tag: str
	operands: tuple[Node, ...]
	line: int
	metadata: tuple[Form, ...] = ()


Node = Atom | Form

# A raw literal cannot close on the first byte of a doubled backtick.
TOKEN = re.compile(
	r'(?P<space>\s+)|(?P<open>\()|(?P<close>\))|'
	r'(?P<string>"(?:\\[^\n]|[^"\\\n])*"|`(?:``|[^`])*`(?!`))|'
	r'(?P<atom>[^\s();"`]+)|(?P<invalid>.)'
)
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_.-]*")
TAG = re.compile(":?" + IDENTIFIER.pattern)


# Recognize metadata entries without interpreting their payloads.
def is_metadata(node: Node) -> bool:
	return isinstance(node, Form) and node.tag.startswith(":")


# Identify a C bridge provider even when leading metadata occupies its slot.
def c_header_provider_index(children: list[Node], index: int = 0) -> int | None:
	# Find the provider solely to distinguish C bridge syntax from opaque providers.
	while index < len(children) and is_metadata(children[index]):
		index += 1
	# An empty remainder cannot identify a provider.
	if index == len(children):
		return None
	provider = children[index]
	return index if isinstance(provider, Atom) and not provider.quoted and provider.text == "c-header" else None


# Follow only declaration containers when deciding whether a form owns a name.
def declaration_context(parents: list[tuple[int, list[Node]]], tag: str) -> bool:
	is_function = tag == "fn"
	in_bridge = False
	# The synthetic accumulator has no tag and can host a standalone source form.
	for depth in range(len(parents) - 1, 0, -1):
		children = parents[depth][1]
		head = children[0] if children else None
		# A malformed or opaque enclosing form cannot establish declaration context.
		if not isinstance(head, Atom) or head.quoted:
			return False
		# A module contributes declarations after its name at the source root.
		if head.text == "module":
			name = children[1] if len(children) > 1 else None
			return depth == 1 and isinstance(name, Atom) and not name.quoted and bool(IDENTIFIER.fullmatch(name.text))
		# Declaration groups are transparent within a declaration ancestry.
		if head.text == "decls":
			continue
		# A supported C bridge contributes members after its provider and header.
		if not is_function or head.text != "bridge" or in_bridge:
			return False
		# The provider identifies member declarations while the enclosing form checks its opening.
		if c_header_provider_index(children, 1) is None:
			return False
		in_bridge = True
	# Standalone functions and C bridges also have fixed opening slots.
	return is_function or tag == "bridge"


# Separate metadata from the logical operands of a completed form.
def read_form(children: list[Node], line: int, parents: list[tuple[int, list[Node]]]) -> Form:
	# Every opener needs a bare tag.
	if not children:
		raise SourceError(line)
	head, *children = children
	tag = identifier(head, TAG)
	# The old attachment forms are reserved so they fail at their old spelling.
	if tag in ("with-meta", "meta"):
		raise SourceError(head.line)
	named = (tag == "module" and len(parents) == 1) or (
		tag in ("fn", "let") and declaration_context(parents, tag)
	)
	metadata = []
	operands = []
	# A declaration's fixed name slot precedes any metadata entries.
	if named:
		# With no operand, the missing name points to the declaration opener.
		if not children:
			raise SourceError(line)
		identifier(children[0])
		operands.append(children[0])
		children = children[1:]
	# C bridge documentation and pragmas follow the provider and quoted header.
	if tag == "bridge" and declaration_context(parents, tag):
		index = c_header_provider_index(children)
		# Provider-specific data stays opaque for the other bridge forms.
		if index is not None:
			# A leading entry occupies the fixed provider slot.
			if index != 0:
				raise SourceError(children[0].line)
			# A header with no operand points to the bridge opener.
			if len(children) < 2:
				raise SourceError(line)
			header = children[1]
			# Point a non-string header or interleaved entry to its original operand.
			if not isinstance(header, Atom) or not header.quoted:
				raise SourceError(header.line)
			operands.extend(children[:2])
			children = children[2:]
	prefix_open = True
	interleaved = named and tag == "fn"
	# Function metadata can appear between ordinary fields or after the last one.
	for child in children:
		# Metadata is attached to the declaration independently of its name and fields.
		if is_metadata(child):
			# Other forms attach metadata only in a contiguous prefix.
			if not (prefix_open or interleaved):
				raise SourceError(child.line)
			metadata.append(child)
		else:
			prefix_open = False
			operands.append(child)
	return Form(tag, tuple(operands), line, tuple(metadata))


# Read one IR tree and record each form's opening line.
def parse(source: str) -> Form:
	# Keep logical lines and raw string values consistent with file input.
	source = source.replace("\r\n", "\n").replace("\r", "\n")
	stack: list[tuple[int, list[Node]]] = [(1, [])]
	line = 1
	# Fold tokens into the current form while tracking source lines.
	for match in TOKEN.finditer(source):
		token = match.group()
		kind = match.lastgroup
		# Count newlines while skipping inter-token whitespace.
		if kind == "space":
			line += token.count("\n")
			continue
		# Start a new operand list at the opening token.
		if kind == "open":
			stack.append((line, []))
			continue
		# Complete the innermost form when its delimiter closes.
		if kind == "close":
			# The outer accumulator has no matching delimiter.
			if len(stack) == 1:
				raise SourceError(line)
			start, children = stack.pop()
			node = read_form(children, start, stack)
		# Raw strings collapse doubled backticks and retain the remaining text.
		elif kind == "string" and token.startswith("`"):
			node = Atom(token[1:-1].replace("``", "`"), line, quoted=True)
			line += token.count("\n")
		# Double-quoted strings continue to decode JSON escapes.
		elif kind == "string":
			# Decode escapes before storing the string value.
			try:
				node = Atom(json.loads(token), line, quoted=True)
			# Point malformed escapes back to their token.
			except ValueError:
				raise SourceError(line) from None
		# Keep unquoted text for the operation that consumes it.
		elif kind == "atom":
			node = Atom(token, line)
		# Reject characters that match no token kind.
		else:
			raise SourceError(line)
		stack[-1][1].append(node)

	# Report the opening line of an unfinished form.
	if len(stack) != 1:
		raise SourceError(stack[-1][0])
	roots = stack[0][1]
	# The seed consumes exactly one root form.
	if len(roots) != 1:
		raise SourceError(roots[1].line if roots else 1)
	root = form(roots[0])
	# The source root has no enclosing form to own a metadata entry.
	if root.tag.startswith(":"):
		raise SourceError(root.line)
	return root


# Require the form and operand count used by an IR operation.
def form(node: Node, tag: str | None = None, arity: int | None = None) -> Form:
	# Atoms have no tag or operands to match.
	if not isinstance(node, Form):
		raise SourceError(node.line)
	# A fixed tag selects the operation the caller expects.
	if tag is not None and node.tag != tag:
		raise SourceError(node.line)
	# Check the operand count before the caller unpacks it.
	if arity is not None and len(node.operands) != arity:
		raise SourceError(node.line)
	return node


# Read an identifier or a form tag with its permitted symbol spelling.
def identifier(node: Node, pattern: re.Pattern[str] = IDENTIFIER) -> str:
	# Only form heads accept the optional metadata prefix.
	if not isinstance(node, Atom) or node.quoted or not pattern.fullmatch(node.text):
		raise SourceError(node.line)
	return node.text

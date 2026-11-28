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
	names: tuple[Atom, ...] = ()
	tag_line: int = 0


Node = Atom | Form

# A raw literal cannot close on the first byte of a doubled backtick.
TOKEN = re.compile(
	r'(?P<space>\s+)|(?P<open>\()|(?P<close>\))|'
	r'(?P<string>"(?:\\[^\n]|[^"\\\n])*"|`(?:``|[^`])*`(?!`))|'
	r'(?P<atom>[^\s();"`]+)|(?P<invalid>.)'
)
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
WORD = re.compile(r"[A-Za-z_][A-Za-z0-9_.-]*")
QUALIFIED_PART = r"[A-Za-z_][A-Za-z0-9_-]*"
TAG = re.compile(":?" + WORD.pattern + "|" + QUALIFIED_PART + ":" + QUALIFIED_PART)
NUL_OR_ESCAPED_BACKSLASH = re.compile(r"\\\\|\\0")


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


# Read a bare enclosing head without treating data forms as declarations.
def open_head(children: list[Node]) -> str | None:
	head = children[0] if children else None
	return head.text if isinstance(head, Atom) and not head.quoted else None


# Follow only declaration containers back to the source root.
def declaration_context(parents: list[tuple[int, list[Node]]], tag: str) -> str | None:
	in_bridge = False
	# The synthetic accumulator has no tag.
	for depth in range(len(parents) - 1, 0, -1):
		children = parents[depth][1]
		head = open_head(children)
		# Only the root module can introduce declaration ancestry.
		if head == "module":
			name = children[1] if len(children) > 1 else None
			# The module's own check reports a malformed name.
			if depth == 1 and isinstance(name, Atom) and not name.quoted and IDENTIFIER.fullmatch(name.text):
				return "c" if in_bridge else "wove"
			return None
		# Groups and disabled declarations preserve the enclosing namespace.
		if head in ("decls", "disabled"):
			continue
		# Only C header bridges give their members this declaration grammar.
		if head != "bridge" or in_bridge or tag not in ("fn", "struct", "type", "const", "let", "let-mut", "snapshot"):
			return None
		index = c_header_provider_index(children, 1)
		# Recognize the provider before its closing form checks the header slots.
		if index is None:
			return None
		in_bridge = True
	# Standalone named types, functions and bridges can be inspected as source forms.
	if in_bridge or tag in ("fn", "struct", "enum", "type", "bridge"):
		return "c" if in_bridge else "wove"
	return None


# Locate a body of an actual Wove function without looking through metadata.
def in_function_body(parents: list[tuple[int, list[Node]]]) -> bool:
	# A nested body can contain expression forms as well as statement blocks.
	for depth in range(len(parents) - 1, 1, -1):
		head = open_head(parents[depth][1])
		# Metadata entries own opaque payload forms.
		if head is None or head.startswith(":"):
			return False
		# The body field belongs to a function only in a Wove declaration context.
		if head == "body" and open_head(parents[depth - 1][1]) == "fn":
			return declaration_context(parents[:depth - 1], "fn") == "wove"
	return False


# Return the number of opening names owned by this occurrence of a tag.
def name_slots(tag: str, children: list[Node], parents: list[tuple[int, list[Node]]]) -> int:
	# Only the source root declares a module.
	if tag == "module":
		return int(len(parents) == 1)
	# Ordinary declarations and C bridge members have explicit name slots.
	if tag in ("fn", "struct", "enum", "type", "const", "let", "let-mut", "snapshot"):
		context = declaration_context(parents, tag)
		# Constants, mutable bridge bindings and snapshots use this slot only in C bridges.
		if context == "c" or (context == "wove" and tag not in ("const", "let-mut", "snapshot")):
			return 1
	# Unrelated tags cannot introduce a local or generic parameter here.
	if len(parents) < 2:
		return 0
	parent = parents[-1][1]
	head = open_head(parent)
	# Only the first ordinary field of a Wove function or struct supplies its generic parameters.
	if head == "generic" and tag in ("type", "const", "fn") and len(parents) >= 3:
		owner = parents[-2][1]
		owner_head = open_head(owner)
		# At this point the enclosing generic form has not been appended to its owner.
		if (owner_head in ("fn", "struct") and len(owner) >= 2
				and all(is_metadata(item) for item in owner[2:])
				and declaration_context(parents[:-2], owner_head) == "wove"):
			return 1
	# Binding-shaped payloads outside a function body keep ordinary form grammar.
	if tag not in ("let", "let-mut") or not in_function_body(parents):
		return 0
	# The first child of a for header binds one or two names before any metadata.
	if head == "for" and tag == "let" and all(is_metadata(item) for item in parent[1:]):
		return 2 if len(children) >= 2 and not is_metadata(children[1]) else 1
	# These blocks declare ordinary local bindings and match payload binders.
	return int(head in ("body", "then", "else", "case"))


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
	slots = name_slots(tag, children, parents)
	names = []
	metadata = []
	operands = []
	# A declaration's fixed name slot precedes any metadata entries.
	for index in range(slots):
		# With no operand, the missing name points to the declaration opener.
		if index >= len(children):
			raise SourceError(line)
		name = children[index]
		# Point misplaced metadata at the original colon-prefixed tag.
		if is_metadata(name):
			raise SourceError(name.tag_line)
		identifier(name)
		names.append(name)
	children = children[slots:]
	# A binder's optional second name must precede its metadata.
	if (tag == "let" and slots == 1 and open_head(parents[-1][1]) == "for"
			and children and is_metadata(children[0]) and any(not is_metadata(child) for child in children)):
		raise SourceError(children[0].tag_line)
	# C bridge documentation and pragmas follow the provider and quoted header.
	if tag == "bridge" and declaration_context(parents, tag):
		index = c_header_provider_index(children)
		# Provider-specific data stays opaque for the other bridge forms.
		if index is not None:
			# A leading entry occupies the fixed provider slot.
			if index != 0:
				raise SourceError(children[0].tag_line)
			# A header with no operand points to the bridge opener.
			if len(children) < 2:
				raise SourceError(line)
			header = children[1]
			# Point a non-string header or interleaved entry to its original operand.
			if not isinstance(header, Atom) or not header.quoted:
				raise SourceError(header.tag_line if is_metadata(header) else header.line)
			operands.extend(children[:2])
			children = children[2:]
	prefix_open = True
	interleaved = bool(slots)
	# Named forms can attach metadata between ordinary fields or after the last one.
	for child in children:
		# Metadata is attached to the declaration independently of its name and fields.
		if is_metadata(child):
			# Other forms attach metadata only in a contiguous prefix.
			if not (prefix_open or interleaved):
				raise SourceError(child.tag_line)
			metadata.append(child)
		else:
			prefix_open = False
			operands.append(child)
	return Form(tag, tuple(operands), line, tuple(metadata), tuple(names), head.line)


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
		# Double-quoted Wove strings add NUL to the JSON escape set.
		elif kind == "string":
			# Consume escaped backslashes together so a following zero stays literal.
			json_token = NUL_OR_ESCAPED_BACKSLASH.sub(
				lambda escape: r"\u0000" if escape[0] == r"\0" else escape[0], token
			)
			# Decode escapes before storing the string value.
			try:
				node = Atom(json.loads(json_token), line, quoted=True)
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


# Read a user name or a fixed language word with its own spelling rules.
def identifier(node: Node, pattern: re.Pattern[str] = IDENTIFIER) -> str:
	# The caller supplies the separate word grammar for fixed syntax.
	if not isinstance(node, Atom) or node.quoted or not pattern.fullmatch(node.text):
		raise SourceError(node.line)
	return node.text

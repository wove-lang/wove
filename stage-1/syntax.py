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


# Separate the initial metadata entries from the operands of a completed form.
def read_form(children: list[Node], line: int) -> Form:
	# Every opener needs a bare tag.
	if not children:
		raise SourceError(line)
	head, *children = children
	tag = identifier(head, TAG)
	# The old attachment forms are reserved so they fail at their old spelling.
	if tag in ("with-meta", "meta"):
		raise SourceError(head.line)
	metadata = []
	operands = []
	# Once an operand appears, later colon-tagged children cannot attach.
	for child in children:
		# Metadata entries are forms with colon-prefixed heads, never colon atoms.
		if isinstance(child, Form) and child.tag.startswith(":"):
			# The prefix is the only valid site for a colon-tagged form.
			if operands:
				raise SourceError(child.line)
			metadata.append(child)
		# Atoms and ordinary forms start or extend the operand list.
		else:
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
			node = read_form(children, start)
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

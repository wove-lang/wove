# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Read tagged Wove IR forms and keep their source lines."""

from __future__ import annotations

from dataclasses import dataclass
import json
import re


class SourceError(Exception):
    def __init__(self, line: int):
        self.line = line
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


Node = Atom | Form

TOKEN = re.compile(
    r'(?P<space>\s+)|(?P<comment>;;[^\n]*)|(?P<open>\()|(?P<close>\))|'
    r'(?P<string>"(?:\\[^\n]|[^"\\\n])*")|(?P<atom>[^\s();"]+)|(?P<invalid>.)'
)
IDENTIFIER = re.compile(r"[A-Za-z_][A-Za-z0-9_.-]*")


def parse(source: str) -> Form:
    """Read one IR tree; the opening parenthesis sets each form's line."""
    stack: list[tuple[int, list[Node]]] = [(1, [])]
    line = 1
    for match in TOKEN.finditer(source):
        token = match.group()
        kind = match.lastgroup
        if kind in ("space", "comment"):
            line += token.count("\n")
            continue
        if kind == "open":
            stack.append((line, []))
            continue
        if kind == "close":
            if len(stack) == 1:
                raise SourceError(line)
            start, children = stack.pop()
            if not children:
                raise SourceError(start)
            tag, *operands = children
            node = Form(identifier(tag), tuple(operands), start)
        elif kind == "string":
            try:
                node = Atom(json.loads(token), line, quoted=True)
            except ValueError:
                raise SourceError(line) from None
        elif kind == "atom":
            node = Atom(token, line)
        else:
            raise SourceError(line)
        stack[-1][1].append(node)

    if len(stack) != 1:
        raise SourceError(stack[-1][0])
    roots = stack[0][1]
    if len(roots) != 1:
        raise SourceError(roots[1].line if roots else 1)
    return form(roots[0])


def form(node: Node, tag: str | None = None, arity: int | None = None) -> Form:
    """Require the form and operand count used by an IR operation."""
    if not isinstance(node, Form):
        raise SourceError(node.line)
    if tag is not None and node.tag != tag:
        raise SourceError(node.line)
    if arity is not None and len(node.operands) != arity:
        raise SourceError(node.line)
    return node


def identifier(node: Node) -> str:
    """Read a symbol, including dotted operation and expectation names."""
    if not isinstance(node, Atom) or node.quoted or not IDENTIFIER.fullmatch(node.text):
        raise SourceError(node.line)
    return node.text

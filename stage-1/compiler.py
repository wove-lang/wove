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


def parameter_type(node: Node) -> str:
    """Accept scalar inputs and the two byte slice parameter types."""
    if isinstance(node, Atom) and not node.quoted and node.text == I32:
        return I32
    view = form(node, arity=1)
    if view.tag not in (SHARED, MUT) or identifier(view.operands[0]) != "u8":
        raise SourceError(node.line)
    return view.tag


def read_expectation(node: Node) -> Form:
    """Keep expectation metadata on its function, including issue and fixture IDs."""
    expectation = form(node, "expect", 3)
    name, issue, tests = expectation.operands
    identifier(name)
    url, = form(issue, "issue", 1).operands
    if not isinstance(url, Atom) or not url.quoted:
        raise SourceError(url.line)
    for test in form(tests, "tests").operands:
        identifier(test)
    return expectation


def read_function(node: Node, index: int) -> tuple[str, Function]:
    """Read the signature before lowering calls, including forward calls."""
    declaration = form(node, "fn")
    if len(declaration.operands) < 4:
        raise SourceError(node.line)
    name_node, params_node, returns_node, *metadata, body_node = declaration.operands
    name = identifier(name_node)

    params = {}
    for param in form(params_node, "params").operands:
        field = form(param, arity=1)
        if field.tag in params:
            raise SourceError(param.line)
        params[field.tag] = parameter_type(field.operands[0])
    result, = form(returns_node, "returns", 1).operands
    if identifier(result) != I32 or (name == "main" and params):
        raise SourceError(node.line)

    c_name = "main" if name == "main" else f"wv_seed_fn_{index}"
    expectations = tuple(read_expectation(item) for item in metadata)
    return name, Function(c_name, tuple(params.items()), expectations, form(body_node, "body"))


def c_string(text: str) -> str:
    """Encode a source filename as a C literal, including arbitrary path bytes."""
    parts = ['"']
    for byte in os.fsencode(text):
        if 32 <= byte <= 126 and byte not in b'"\\?':
            parts.append(chr(byte))
        else:
            parts.append(f"\\{byte:03o}")
    parts.append('"')
    return "".join(parts)


def signature(function: Function) -> str:
    """Give each parameter a C name independently of its source spelling."""
    params = ", ".join(
        f"{C_TYPE[kind]} wv_value_{index}"
        for index, (_, kind) in enumerate(function.params)
    )
    result = "int" if function.c_name == "main" else "static int32_t"
    return f"{result} {function.c_name}({params or 'void'})"


class FunctionEmitter:
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

    def temporary(self, kind: str, expression: str) -> Value:
        """Evaluate each operation in source order before emitting its consumer."""
        name = f"wv_value_{self.next_value}"
        self.next_value += 1
        self.lines.append(f"    {C_TYPE[kind]} {name} = {expression};")
        return Value(kind, name)

    def local(self, node: Node, line: int) -> Binding:
        """Resolve a live place and report failures at the operation using it."""
        name, = form(node, "local", 1).operands
        binding = self.bindings.get(identifier(name))
        if binding is None or binding.moved:
            raise SourceError(line)
        return binding

    def i32(self, node: Node) -> Value:
        """Require a scalar in arithmetic, lengths, arguments, and returns."""
        value = self.expression(node)
        if value.kind != I32:
            raise SourceError(node.line)
        return value

    def borrow(self, node: Node, kind: str) -> Value:
        """Hold this loan through the remaining argument evaluations and the call."""
        tag = "borrow-mut" if kind == MUT else "borrow-shared"
        target, = form(node, tag, 1).operands
        binding = self.local(target, node.line)
        if binding.value.kind != OWNED or (kind == MUT and not binding.writable):
            raise SourceError(node.line)

        for owner, prior in self.loans:
            if owner is binding and (kind == MUT or prior == MUT):
                raise SourceError(node.line)
        self.loans.append((binding, kind))
        owner = binding.value.c
        return self.temporary(
            kind, f"({C_TYPE[kind]}){{{owner}->wvrt_items, "
            f"{owner}->wvrt_length, {owner}->wvrt_element_size}}"
        )

    def call(self, node: Form) -> Value:
        """Evaluate arguments left to right and end only this call's loans."""
        if not node.operands:
            raise SourceError(node.line)
        name, *args = node.operands
        callee = self.functions.get(identifier(name))
        if callee is None or callee.c_name == "main" or len(args) != len(callee.params):
            raise SourceError(node.line)
        checkpoint = len(self.loans)
        values = []
        for argument, (_, kind) in zip(args, callee.params):
            value = self.i32(argument) if kind == I32 else self.borrow(argument, kind)
            values.append(value.c)
        result = self.temporary(I32, f"{callee.c_name}({', '.join(values)})")
        del self.loans[checkpoint:]
        return result

    def expression(self, node: Node) -> Value:
        """Translate one operation, preserving ownership of every array value."""
        expr = form(node)
        if expr.tag == I32:
            literal, = form(expr, arity=1).operands
            if not isinstance(literal, Atom) or literal.quoted or not re.fullmatch(r"-?[0-9]+", literal.text):
                raise SourceError(node.line)
            try:
                number = int(literal.text)
            except ValueError:
                raise SourceError(node.line) from None
            if not -(2**31) <= number < 2**31:
                raise SourceError(node.line)
            return Value(I32, str(number))

        if expr.tag == "local":
            binding = self.local(expr, node.line)
            if binding.value.kind != I32:
                raise SourceError(node.line)
            return binding.value

        if expr.tag == "array.alloc":
            element, length = form(expr, arity=2).operands
            if identifier(element) != "u8":
                raise SourceError(node.line)
            size = self.i32(length)
            site = f"(wvrt_location){{{self.source}, {node.line}, 1}}"
            return self.temporary(
                OWNED, f"wvrt_array_new({size.c}, sizeof(uint8_t), _Alignof(uint8_t), {site})"
            )

        if expr.tag == "move":
            target, = form(expr, arity=1).operands
            binding = self.local(target, node.line)
            if binding.value.kind != OWNED or any(owner is binding for owner, _ in self.loans):
                raise SourceError(node.line)
            result = self.temporary(OWNED, binding.value.c)
            binding.moved = True
            self.lines.append(f"    {binding.value.c} = NULL;")
            return result

        if expr.tag == "slice.len":
            target, = form(expr, arity=1).operands
            binding = self.local(target, node.line)
            if binding.value.kind not in (SHARED, MUT):
                raise SourceError(node.line)
            return self.temporary(I32, f"{binding.value.c}.wvrt_length")

        if expr.tag == "i32.add.wrapping":
            left, right = form(expr, arity=2).operands
            first = self.i32(left)
            second = self.i32(right)
            return self.temporary(I32, f"wvrt_i32_add_wrapping({first.c}, {second.c})")

        if expr.tag == "call":
            return self.call(expr)

        raise SourceError(node.line)

    def emit(self) -> list[str]:
        """Emit straight-line bindings and release live owners at the return."""
        body = self.function.body
        if not body.operands:
            raise SourceError(body.line)
        self.lines.append(signature(self.function) + " {")

        *statements, last = body.operands
        for node in statements:
            statement = form(node, arity=2)
            if statement.tag not in ("let", "let-mut"):
                raise SourceError(node.line)
            name, initializer = statement.operands
            symbol = identifier(name)
            if symbol in self.bindings:
                raise SourceError(name.line)
            value = self.expression(initializer)
            self.bindings[symbol] = Binding(value, writable=statement.tag == "let-mut")

        result, = form(last, "return", 1).operands
        value = self.i32(result)
        for binding in reversed(self.bindings.values()):
            if binding.value.kind == OWNED and not binding.moved:
                self.lines.append(f"    wvrt_array_drop({binding.value.c});")
        self.lines.extend([f"    return {value.c};", "}", ""])
        return self.lines


def compile_source(source: str, filename: str) -> str:
    """Emit a complete C translation unit after the entire module succeeds."""
    module = form(parse(source), "module")
    if not module.operands:
        raise SourceError(module.line)
    name, *declarations = module.operands
    identifier(name)
    functions = {}
    for index, node in enumerate(declarations):
        symbol, function = read_function(node, index)
        if symbol in functions:
            raise SourceError(node.line)
        functions[symbol] = function
    if "main" not in functions:
        raise SourceError(module.line)

    lines = ['#include "wove_runtime.h"', ""]
    lines.extend(signature(fn) + ";" for fn in functions.values())
    lines.append("")
    path = c_string(filename)
    for fn in functions.values():
        lines.extend(FunctionEmitter(fn, functions, path).emit())
    return "\n".join(lines)

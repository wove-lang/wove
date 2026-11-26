<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# <strong>wove</strong>

<strong>wove</strong> is an experimental programming language designed for coding
agents. Its self-hosted compiler emits C.

- **A textual AST.** Tagged forms give agents a regular structure to read and edit.
- **Explicit ownership.** Moves and borrows make access visible. The compiler
  handles cleanup.
- **Expectations in the source.** `:expect` links code to tests and issues so agents
  can find what a change must preserve.
- **Structured diagnostics.** Errors include source spans in JSON by default.
  Use `--human` to read them in the terminal.

## Language

[Built-in quick reference](LANGUAGE.md) · [Stage 2 reference](stage-2/README.md)

License options are [MIT](LICENSE-MIT) and [Apache-2.0](LICENSE-APACHE).

## Development

Requires Python 3, Make, and Clang.

```sh
make
```

The compiler is `build/wovec`. Run `make test` for the test suite and
`make self-host` to verify the bootstrap chain.

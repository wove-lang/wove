<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Bootstrap language subset

Stage 2 is written in the subset the Python seed can compile. This reference
records the limits that matter when editing the compiler. The
[Wove language reference](../LANGUAGE.md) describes shared syntax and semantics.

## Forms and selection

Use one named `module` per file. Give each Wove function a name, `params`,
`returns`, and `body`, including explicit `(params)` for a zero-argument
helper. A `void` helper can use `(return)` or fall through.

The seed enables `bootstrap` during selection and excludes `(test)` forms.
It evaluates selection on modules, declarations, and `decls` groups before
opening providers or binding names. All `cfg` predicates on a form must pass.
For example, use one import for the bootstrap driver and one for the native driver.

```lisp
(use (:pragma (cfg bootstrap)) wove-file "./driver/bootstrap/entry.wove"
    (only (as run_bootstrap_command run_compiler)))
(use (:pragma (cfg (not bootstrap))) wove-file "./driver/command.wove"
    (only (as run_native_command run_compiler)))
```

An excluded module contributes no names or nested imports. An empty `(only)`
can visit an excluded provider. A named selection reports the unavailable
name. The entry module must be active. Other directives and metadata
payloads stay opaque to the seed.

## Values and operators

Use `bool`, `i8`, `u8`, `i32`, and `u32`. Integer literals take decimal or
hexadecimal digits, with either a suffix (`0xc2u8`) or a type head
(`(u8 0xC2)`). For a signed negative hexadecimal literal, put the minus
before the prefix (`-0x80i8`). Write bytes as typed integers.

| Family | Available forms |
| --- | --- |
| Wrapping arithmetic | `add`, `sub`, `inc`, and `dec` on `i32` and `u32` |
| Comparisons | `eq`, `lt`, `eqz`, `ltz`, and `gtz` and their `not-` forms on `u8`, `i32`, and `u32` |
| Bitwise operations | `u32.and`, `u32.or`, `u32.xor`, and `u32.not` |
| Shifts | `u32.shl` and `u32.shr` with an exact `i32` count in `[0, 32)` |
| Checked conversions | `(int.convert T value)` and `(T.from-S value)` between the four integer types |
| Boolean logic | `bool.and`, `bool.or`, and `bool.not` |

An immutable module `let` takes a typed integer literal or a quoted byte
string. Read it with `local`. A local or parameter shadows the module name.
A string or raw backtick string gives a static `(shared-array u8)`.

A named `struct` has scalar fields of the types above. Use `struct.new` to
supply every field exactly once and `struct.get` to read fields. Records copy
by value and keep their defining identity, including through import aliases.

## Control flow and ownership

Use `let` for a binding, `let-mut` for a writable scalar or array owner,
and `set` to update an exact scalar binding. Use statement `if` with a
`then` block and optional `else`, and `while` with a `body`. Each scope
drops its remaining owners when it exits. After an `if`, an owner must
be available along every continuing path before it can be used. Each
path back to a `while` condition must preserve owners live on entry.

Use `(array.alloc T count)` for a local owned array with `u8` or a
copyable record as its element type. Use `array.len`, `array.load`, and
`array.store` with exact element types and `i32` indices. A writable
receiver is required for stores. Transfer a named owner with
`(move name)`.

Pass `(shared-array T)` and `(mut-array T)` parameters using
`borrow-shared` and `borrow-mut` on an owner or received view. For a
checked byte subrange, borrow `(array.range name start end)`.
The `i32` endpoints evaluate once in source order, and equal endpoints
are valid. Call loans remain active through later arguments and the call.
Shared reborrows permit reads through the parent. Exclusive reborrows
suspend access through the parent until the call returns.

The [application entry signature](../LANGUAGE.md#program-entry) also applies
to the compiler's `main`.

## Imports and C headers

Use `wove-file` with a path relative to the importer and an explicit
`(only ...)` list. Select functions, immutable module values, or record
types by name or alias. An empty list visits the provider without binding
names. Each function resolves names in its defining file.

An active `(use c-header "<stdint.h>" (only ...))` contributes a system
include. A project header can use `"./local.h"`. Explicit relative paths
and files found beside the declaring module are anchored there. Other
quoted paths use Clang's include search.

The generated C begins with `wove_runtime.h`. Other headers appear once in
first-use order through the depth-first Wove import walk. For an existing
project header, the seed deduplicates by resolved containing directory and
filename and keeps the first include spelling. C-header selections add
no Wove names. Bridge bodies, other foreign providers, and their selections
stay opaque to the seed. The private bootstrap runtime supplies its own
fixed C bindings.

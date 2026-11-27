<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Bootstrap language subset

The shared graph checker and C emitter use the subset the Python seed can
compile. The rest of the compiler builds with that emitter and uses the
full language. The
[Wove language reference](../LANGUAGE.md) describes shared syntax and semantics.

## Forms and selection

Use one named `module` per file. Give each Wove function a name, `params`,
`returns`, and `body`, including explicit `(params)` for a zero-argument
helper. A `void` helper can use `(return)` or fall through.

Put the name immediately after `module`, `fn`, `struct`, `let`, or
`let-mut`. Named forms attach metadata anywhere after the name, including
between operands and after the last operand. For example,
`(let index (:doc "Initial index.") 42i32)` and
`(let index 42i32 (:doc "Initial index."))` declare the same value.
Fields and function parameters still use their name as the head.

The seed excludes test declarations before opening providers or binding
names. The compiler entry uses ordinary imports to reach the same driver
at every stage.

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
| Wrapping arithmetic | `add`, `sub`, `mul`, `inc`, and `dec` on `i32` and `u32`. |
| Division and remainder | `div` and `rem` on `i32` and `u32`. Both trap on a zero divisor. |
| Comparisons | `eq`, `lt`, `eqz`, `ltz`, and `gtz` and their `not-` forms on `u8`, `i32`, and `u32` |
| Bitwise operations | `u32.and`, `u32.or`, `u32.xor`, and `u32.not` |
| Shifts | `u32.shl` and `u32.shr` with an exact `i32` count in `[0, 32)` |
| Checked conversions | `(int.convert T value)` and `(T.from-S value)` between the four integer types |
| Boolean logic | `bool.and`, `bool.or`, and `bool.not` |

An immutable module `let` takes a typed integer literal or a quoted byte
string. Read it by its bare name, as in `(return count)`. A local or
parameter shadows a module binding with the same name. A string or raw
backtick string gives a static `(shared-array u8)`.

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
receiver is required for stores. Fixed scalar arrays can be passed by
value and read with `array.len` and `array.load`. Transfer or return an
owned array with `(move name)`.

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
stay opaque to the Python seed.

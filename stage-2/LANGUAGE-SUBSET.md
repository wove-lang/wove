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

Target predicates select declarations using Clang's OS, architecture, and
deployment. Use `(os "linux")`, `(arch "aarch64")`, or inclusive
`(os_min 270000)` and `(os_max 270000)` bounds. Combine them with
`all`, `any`, and `not`. Deployment bounds are false when no deployment
version is available.

An excluded module contributes no names or nested imports. An empty `(only)`
can visit an excluded provider. A named selection reports the unavailable
name. The entry module must be active. Other directives and metadata
payloads stay opaque to the seed.

## Values and operators

Use `bool`, `i8`, `u8`, `i32`, `u32`, and `u64`. Integer literals take decimal or
hexadecimal digits, with either a suffix (`0xc2u8`) or a type head
(`(u8 0xC2)`). For a signed negative hexadecimal literal, put the minus
before the prefix (`-0x80i8`). Write bytes as typed integers.

| Family | Available forms |
| --- | --- |
| Wrapping arithmetic | `add`, `sub`, `mul`, and `inc` on `i32`, `u32`, and `u64`. `dec` on `i32` and `u32`. |
| Division and remainder | `div` and `rem` on `i32` and `u32`. Both trap on a zero divisor. |
| Comparisons | `eq`, `lt`, `eqz`, `ltz`, and `gtz` and their `not-` forms on `u8`, `i32`, `u32`, and `u64`. `gt` on `u64`. |
| Bitwise operations | `and`, `or`, `xor`, and `not` on `u32` and `u64`. |
| Shifts and rotation | `shl` and `shr` on `u32` and `u64`, and `u64.rotr`. Counts are `i32` and less than the bit width. |
| Checked conversions | `(int.convert T value)` and `(T.from-S value)` between the five integer types |
| Boolean logic | `bool.and`, `bool.or`, and `bool.not` |

An immutable module `let` takes a scalar literal, a quoted byte string,
an `embed`, or a fixed scalar array of literals. Read it by its bare
name, as in `(return count)`. A local or parameter shadows a module binding
with the same name. A string or raw backtick string gives a static
`(shared-array u8)`.

Use `(embed "./data.bin")` at module or function scope to read a file at
compile time as a static `(shared-array u8)`. Relative paths start at the
module containing the embed.

A named `struct` has scalar fields or nested Wove records, including
records selected through imports. Use `struct.new` to supply every field
exactly once and `struct.get` to read fields. Copyable records keep their
defining identity, including through import aliases.

Records can own byte arrays, `i32` arrays, and arrays of owned byte rows.
Initialize an owning field with a fresh or moved value. Use a field
projection such as `(borrow-shared (struct.get job arguments))` to lend
its storage. Move the containing record to transfer its fields. Scope
cleanup drops each remaining owned field.

## Control flow and ownership

Use `let` for a binding, `let-mut` for a writable scalar or array owner,
and `set` to update an exact scalar binding. Use statement `if` with a
`then` block and optional `else`, and `while` with a `body`. Each scope
drops its remaining owners when it exits. After an `if`, an owner must
be available along every continuing path before it can be used. Each
path back to a `while` condition must preserve owners live on entry.

Use `(array.alloc T count)` for a local owned array with `u8`, `i32`,
owned byte rows, or a Wove record as its element type. Use `array.len`,
`array.load`, and `array.store` with exact element types and `i32` indices.
A writable receiver is required for stores. Transfer or return an owned
array with `(move name)`.

A function can return `(optional (owned-array u8))`. Use
`(array.try-alloc u8 length)` for fallible allocation,
`(optional.none (owned-array u8))` for absence, or `(optional.some value)`
to wrap a fresh or moved byte array. Check a named optional with
`owner.is-null`. Use `(move (optional.unwrap name))` to check presence
and transfer its payload. An optional that still owns a payload drops
it at scope exit.

Use `(array.new (array T N) item ...)` to initialize a fixed scalar array
with exactly `N` items, or `(array.repeat (array T N) item)` to evaluate
an item once and fill the array with it. Fixed scalar arrays support
the same length, load, and store operations and can also be passed by value.

Pass `(shared-array T)` and `(mut-array T)` parameters using
`borrow-shared` and `borrow-mut` on an owner, received view, or fixed array. For a
checked byte subrange, borrow `(array.range name start end)`.
The `i32` endpoints evaluate once in source order, and equal endpoints
are valid. Call loans remain active through later arguments and the call.
Shared reborrows permit reads through the parent. Exclusive reborrows
suspend access through the parent until the call returns.

Use `(let view (borrow-shared (array.range source start end)))` to keep a
shared range of an owner or received view for the enclosing block. The
bound view keeps the original element type. Its loan permits reads and
shared reborrows and prevents writes or moves through the source until
the binding's block exits.

A `(shared-ref Record)` parameter accepts a shared borrow of an existing
record or array element. Its fields can be read or borrowed from the
original storage. An array of owning records starts with empty slots.
A store initializes or replaces a slot, dropping its previous value.
Borrowing an empty slot traps.

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
filename and keeps the first include spelling.

An explicit `(type Name)` selection from a C bridge binds a typedef or record
to the C spelling in its `c-type` field. A selected C record can be passed
by value, initialized as a C aggregate, and read through scalar fields or
native named fields from the same bridge. `int.convert` uses Clang's native type to check the
range of a conversion involving a C integer typedef. C bridge functions,
values, and other foreign providers stay opaque to the Python seed.

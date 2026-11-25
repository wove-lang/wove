<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Wove builtin quick reference

Spellings are case-sensitive. Integer literals carry their type, as in `(i32 -3)`
and `(u32 42)`. Both stages support `i8`, `u8`, `i32`, `u32`, and `bool`.
Stage 2 also supports `u64`. Boolean literals are `(bool true)` and `(bool false)`.

## Integers and booleans

In `(i32.add.wrapping left right)`, the prefix selects the exact operand type.

| Operation | Both stages | Stage 2 additions |
| --- | --- | --- |
| Wrapping addition | `i32.add.wrapping`, `u32.add.wrapping` | `u64.add.wrapping` |
| Wrapping subtraction | `i32.sub.wrapping`, `u32.sub.wrapping` | |
| Wrapping multiplication | | `i32.mul.wrapping` |
| Division and remainder | | `i32.div`, `i32.rem` |
| Equality | `u8.eq`, `i32.eq`, `u32.eq` | `u64.eq`, `(int.eq Type left right)` |
| Less than | `u8.lt`, `i32.lt`, `u32.lt` | `u64.lt` |
| AND, OR, XOR, complement | `u32.and`, `u32.or`, `u32.xor`, `u32.not` | `u64.and`, `u64.or`, `u64.xor`, `u64.not` |
| Left and logical right shift | `u32.shl`, `u32.shr` | `u64.shl`, `u64.shr` |
| Boolean logic | `bool.and`, `bool.or`, `bool.not` | |

- Comparisons return `bool`. `int.eq` also accepts a selected native integer
  alias as its exact type. `bool.and` and `bool.or` short-circuit.
- A shift takes an `i32` count. Counts outside `[0, width)` trap.
- Division truncates toward zero. A zero divisor traps for division and
  remainder. `INT32_MIN / -1` traps, and `INT32_MIN % -1` is zero.
- `(int.convert Type value)` converts a fitting integer and traps on an
  out-of-range value. Stage 2 also supports conversions to and from native
  integer aliases. The seed accepts `(i32.from-u8 byte)` as well.

## Values and storage

| Form | Meaning |
| --- | --- |
| `(local name)` | Read a local, parameter, or visible immutable module binding. |
| `(call name args...)` | Call a visible function. Arguments evaluate left to right. |
| `(set (local name) value)` | Replace a mutable local with a value of its exact type. |
| `(set (record.get place field) value)` | Stage 2. Replace an exact field through a mutable Wove record local, including along a nested field path. |
| `(record.make Type (field value) ...)` | Construct a record from typed field values in written order. |
| `(record.get value field)` | Read a field with its declared type. |
| `(array.alloc T count)` | Allocate an owned, zeroed buffer of `u8` or a supported copyable record type. `count` is `i32`. |
| `(move (local buffer))` | Transfer ownership of a local buffer. |
| `(embed "./payload.bin")` | Stage 2. Initialize a fresh owned byte buffer from a file relative to the defining module. |

A quoted string in an immutable module binding is a shared `u8` slice.
Its length counts the decoded bytes.

## Arrays, slices, and borrows

| Form | Meaning |
| --- | --- |
| `(slice.len view)` | Count elements as `i32`. |
| `(slice.load view index)` | Read one element by value. |
| `(slice.store view index value)` | Write through a mutable view. Stage 2 also accepts a mutable local byte owner. |
| `(borrow-shared (local data))` | Lend read access to a call. |
| `(borrow-mut (local data))` | Lend exclusive write access to a call from writable storage. |
| `(borrow-shared (slice.range (local data) start end))` | Lend a half-open range of a named byte owner or shared byte slice to a call. |

Slice and array indexes and range endpoints are exact `i32` values.
Bounds violations trap. A call loan lasts through later arguments and the call.

Stage 2 fixed arrays keep their elements inline and copy by value. Use
`(array T extent)` for the type, with a builtin scalar `T` and a nonnegative
decimal extent.

- `(array.make (array u8 2) (u8 17) (u8 25))` lists the elements.
- `(array.repeat (array u8 128) (u8 0))` evaluates one value and fills the array.
- `(array.load value index)` reads an element.
- `(array.store (local data) index value)` writes into a mutable local array.

Stage 2 can borrow fixed `u32` and `u64` local arrays as matching slices.
A Stage 2 function with result type `(owned-array u8)` can return a moved
local with `(return (move (local buffer)))` or return a fresh owned call result.

[Seed reference](stage-1/README.md) · [Stage 2 reference](stage-2/README.md)

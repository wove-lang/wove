<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Wove builtin quick reference

Spellings are case-sensitive. Decimal integer atoms carry their type, as in
`-3i32` and `42u32`. The typed lists `(i32 -3)` and `(u32 42)` are
equivalent. Both stages support `i8`, `u8`, `i32`, and `u32`. Stage 2 also
supports `u64`. Boolean literals are `(bool true)` and `(bool false)`.

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
- Stage 2 also accepts typed checked forms such as `(u8.from.u32 value)`.
  The operand must have the exact source type named in the tag.
- Stage 2 integer byte conversions use `T.to.le-bytes`, `T.to.be-bytes`,
  `T.from.le-bytes`, and `T.from.be-bytes`. They use fixed-width
  two's-complement encodings for signed types. Encoding produces a fixed byte
  array, and decoding requires a shared byte view of exactly that width.

## Values and storage

| Form | Meaning |
| --- | --- |
| `(local name)` | Read a local, parameter, or visible immutable module binding. |
| `(call name args...)` | Call a visible function. Arguments evaluate left to right. |
| `(set (local name) value)` | Replace a mutable local with a value of its exact type. |
| `(set (struct.get place field) value)` | Stage 2. Replace an exact field through a mutable Wove record local or reference, including along a nested field path. |
| `(struct.new Type (field value) ...)` | Construct a record from typed field values in written order. |
| `(struct.get value field)` | Read a field with its declared type. |
| `(array.alloc T count)` | Allocate an owned, zeroed buffer of `u8` or a supported copyable record type. `count` is `i32`. |
| `(move (local buffer))` | Transfer ownership of a local buffer or, in Stage 2, hand a present native owner to a consuming C call. |
| `(owner.is-null (local file))` | Stage 2. Test whether a native owner holds a missing handle. |
| `(embed "./payload.bin")` | Stage 2. Initialize a fresh owned byte buffer from a file relative to the defining module. |

A quoted string in an immutable module binding is a `(shared-array u8)` view.
Its length counts the decoded bytes. A Stage 2 immutable module binding
can also declare an inline table with `array.new` or `array.repeat`.
Its elements are exact scalar literals or constant constructors of a
named Wove record. `array.load` can read the table directly.

In Stage 2, `(shared-ref Type)` and `(mut-ref Type)` form record parameter types.
Use `borrow-shared` or `borrow-mut` on a named record place to lend it for a call.

## Fieldless enums

Stage 2 uses `(enum Status (variants (Ok) (IoError) (Invalid)))` to
declare a nominal, copyable type. `(enum.new Status Ok)` constructs one
variant of that type. `(match value (case Ok statements...) ...)` is a
statement that evaluates its subject once, requires one case for every
variant, and runs only the selected case in its own scope.

## Arrays, slices, and borrows

| Form | Meaning |
| --- | --- |
| `(owned-array T)` | Own the backing allocation. |
| `(shared-array T)` | Borrow read access to an array view. |
| `(mut-array T)` | Borrow exclusive write access to an array view. |
| `(array.len data)` | Count the elements in a live owner, fixed array, or received view as `i32`. |
| `(array.load data index)` | Read an element by value from a readable receiver. |
| `(array.store data index value)` | Write an exact element value through a mutable view or into writable array storage. |
| `(borrow-shared (local data))` | Lend read access to the whole array for a call. |
| `(borrow-mut (local data))` | Lend exclusive write access to the whole array for a call. |
| `(borrow-shared (array.range (local data) start end))` | Lend a half-open byte range of a named owner or view. |
| `(borrow-mut (array.range (local data) start end))` | Lend a half-open byte range from a mutable owner or exclusive view. |

Indexes and range endpoints are exact `i32` values. Received views count
and index elements relative to their own bounds. Bounds violations trap.
A call loan lasts through later arguments and the call.

Stage 2 fixed arrays keep their elements inline and copy by value. Use
`(array T extent)` for the type, with a builtin scalar `T` and a nonnegative
decimal extent.

- `(array.new (array u8 2) (u8 17) (u8 25))` lists the elements.
- `(array.repeat (array u8 128) (u8 0))` evaluates one value and fills the array.
- `(array.load value index)` reads an element.
- `(array.store (local data) index value)` writes into a mutable local array.
- `(array.store (struct.get (local state) block) index value)` writes into
  an inline array field of a writable record.

Stage 2 can borrow fixed integer arrays as matching slices. Supported element
types are `i8`, `u8`, `i32`, `u32`, and `u64`.
A fixed integer array field can also lend a matching slice
with `(borrow-shared (struct.get (local state) block))` or the mutable form.
Sibling field paths can lend independently. A whole-record loan overlaps
all of its fields.
A Stage 2 function with result type `(owned-array u8)` can return a moved
local with `(return (move (local buffer)))` or return a fresh owned call result.

[Seed reference](stage-1/README.md) · [Stage 2 reference](stage-2/README.md)

<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Wove builtin quick reference

Spellings are case-sensitive. Decimal integer atoms carry their type, as in
`-3i32` and `42u32`. The typed lists `(i32 -3)` and `(u32 42)` are
equivalent. Both stages support `i8`, `u8`, `i32`, and `u32`. Stage 2 also
supports `i16`, `u16`, `i64`, and `u64`. Boolean literals are `(bool true)` and `(bool false)`.

## Integers and booleans

Typed numeric operators use `type.operation`, with hyphens inside compound
operation names. In `(i32.add left right)`, the prefix selects the exact
operand type. Integer `add`, `sub`, and `mul` wrap at the declared width.

| Operation | Both stages | Stage 2 additions |
| --- | --- | --- |
| Wrapping addition | `i32.add`, `u32.add` | All integer widths |
| Wrapping subtraction | `i32.sub`, `u32.sub` | All integer widths |
| Wrapping multiplication | | All integer widths |
| Checked arithmetic | | `add-checked`, `sub-checked`, and `mul-checked` at all integer widths |
| Division and remainder | | `div` and `rem` at all integer widths |
| Equality | `u8.eq`, `i32.eq`, `u32.eq` | All integer widths and `(int.eq Type left right)` |
| Ordering | `u8.lt`, `i32.lt`, `u32.lt` | `lt`, `le`, `gt`, and `ge` at all integer widths |
| Compare with zero | | `eqz`, `ltz`, `lez`, `gtz`, and `gez` at all integer widths |
| Negated comparison | | Prefix the predicate with `not-`, as in `u32.not-eq`, `i32.not-ltz`, or `int.not-eq` |
| AND, OR, XOR, complement | `u32.and`, `u32.or`, `u32.xor`, `u32.not` | `i32.and`, `i32.or`, `u64.and`, `u64.or`, `u64.xor`, `u64.not` |
| Left and right shift | `u32.shl`, `u32.shr` | `i32.shl`, `i32.shr`, `u64.shl`, `u64.shr` |
| Rotate right | | `u64.rotr` |
| Boolean logic | `bool.and`, `bool.or`, `bool.not` | |

- Integer arithmetic evaluates both operands once, from left to right.
  Their types and the result type must match the type named in the tag.
  Checked arithmetic traps if the mathematical result is outside that type's
  range, including unsigned subtraction below zero. The diagnostic points
  to the operation head.
- Comparisons return `bool` and evaluate each written operand once, from
  left to right. A zero predicate compares with zero of the selected type,
  including when the result is fixed by unsigned ordering. `int.eq` and
  `int.not-eq` also accept a selected native integer alias as their exact
  type. `bool.and` and `bool.or` short-circuit.
- A shift takes an `i32` count. Counts outside `[0, width)` trap. Left shifts
  keep the low bits of the declared width. Right shifts extend the sign for
  signed integers and fill with zeros for unsigned integers.
- `(u64.rotr word count)` rotates a `u64` right by an `i32` count in
  `[0, 64)`. A zero count returns the word unchanged. Out-of-range counts
  trap at the count operand's source span.
- Division truncates toward zero, and a nonzero remainder has the sign of
  the dividend. Both operations trap on zero divisors. Dividing a signed
  minimum by `-1` traps, and its remainder by `-1` is zero. Traps point to
  the full operation span.
- `(int.convert Type value)` converts a fitting integer and traps on an
  out-of-range value. Stage 2 also supports conversions to and from native
  integer aliases. The seed accepts `(i32.from-u8 byte)` as well.
- Stage 2 also accepts typed checked forms such as `(u8.from-u32 value)`.
  The operand must have the exact source type named in the tag.
- `(T.fits value)` returns whether an integer fits the built-in integer type
  `T`. Stage 2 evaluates the input once.
- `(T.try_add left right (borrow-mut place))` and the corresponding
  `try_sub`, `try_mul`, `try_div`, and `try_rem` forms evaluate exact
  `T` values left to right, then borrow the writable `T` place. They
  return `true` and store a representable result. Overflow or a zero
  divisor returns `false` and preserves the destination. Dividing a signed
  minimum by `-1` fails. Its remainder by `-1` succeeds with zero.
- Stage 2 integer byte conversions use `T.to-le-bytes`, `T.to-be-bytes`,
  `T.from-le-bytes`, and `T.from-be-bytes`. They use fixed-width
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
| `(array.alloc T count)` | Allocate an owned, zeroed buffer. Stage 2 supports numeric and copyable record elements. `count` is `i32`. |
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
A `(mut-ref T)` parameter can also borrow a writable place of an exact
built-in scalar type. In the callee, `(local name)` reads the scalar and
`(set (local name) value)` updates the caller's storage.

In Stage 2, `(if condition (then value) (else value))` can be used as a
value. The condition must be `bool`, and both arms must contain exactly one
expression of the same type. The condition runs once and only the selected arm
runs. A conditional record or fixed array supports direct reads. A value arm
can produce a fresh owner. Transferring an existing binding still follows the
usual move rules.

## Enums

Stage 2 uses `(enum Status (variants (Ok) (IoError) (Invalid)))` to
declare a nominal, copyable type. `(enum.new Status Ok)` constructs one
variant of that type. `(match value (case Ok statements...) ...)`
evaluates its subject once and runs only the selected case in its own
scope. Cover all enum variants or use a final `(case _ ...)` for
uncovered variants. In an enum that declares a variant named `_`, that
spelling continues to select the declared variant.

A variant can carry one value. Declare it as `(Push i64)`, build it
with `(enum.new Instruction Push value)`, and match it with
`(case Push (bind word) statements...)`. The payload must have the
variant's exact declared type. The immutable binding is scoped to that arm.
A copyable payload is copied into its binding. Use `(bind _)` to discard it.

An enum with an owning variant is an owner. A statement match can consume
a fresh enum or an explicit `(move (local name))`. The selected arm owns
the payload and drops it on exit unless it moves again. A discard or
wildcard also releases any active owner at arm exit.

A built-in integer match uses literal patterns and requires a final
`(case _ ...)`. Bare decimal and hexadecimal patterns take the subject's
type. An explicit type, as in `1u32` or `(u32 1)`, must agree exactly
with the subject. Duplicate integer values are errors across spellings.

In a value match, each case provides one expression of the same exact
copyable type after its pattern and any payload binding. The selected
expression supplies the result.

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

Stage 2 growable arrays use a named owner or a field of a named place
as their receiver. Mutation requires writable storage with no overlapping
live loan.

| Form | Meaning |
| --- | --- |
| `(array.capacity data)` | Read the reserved element count as `i32`. |
| `(array.push data value)` | Append one value of the exact element type, growing as needed. |
| `(array.remove data index)` | Return the element at an `i32` index and shift later elements left. |
| `(array.truncate data length)` | Keep the first `length` live elements, with no capacity change. |
| `(array.reserve data capacity)` | Ensure room for the requested `i32` capacity, trapping on failure. |
| `(array.try-reserve data capacity)` | Return `false` for an invalid capacity or allocation failure, preserving the array. |

Push captures its value before changing storage, so it can append a
copy of a live element from the same array. Reserving capacity leaves
the logical length unchanged. Removal and truncation use the logical
length for bounds and retain the reserved storage.

An array of owned byte rows accepts
`(array.push (local rows) (move (local row)))`. A successful push
transfers the original byte owner to the vector. Removing a populated
row transfers it to the result, and removing an unfilled slot traps.
Truncation drops the owners in the removed suffix immediately.


Stage 2 fixed arrays keep their elements inline and copy by value. Use
`(array T extent)` for a builtin scalar or copyable Wove record `T`.
The extent is a nonnegative decimal or an immutable module integer.

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

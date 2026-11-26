<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Wove language reference

## First program

Wove source uses tagged forms. A file contains one named `module`. Each
Wove function definition has a name and exactly one `params`, `returns`, and `body` form.
Spell `(params)` for a zero-argument helper. Tags and names are case-sensitive.

```lisp
(module example
    (struct ArgSpan (fields (start i32) (end i32)))

    (fn twice (params (value i32)) (returns i32)
        (body (return (i32.add (local value) (local value)))))

    (fn main (params (bytes (shared-array u8)) (args (shared-array ArgSpan))) (returns i32)
        (body (return (call twice 21i32)))))
```

Save this as `example.wove`, then build and run it.

```sh
wovec bin example.wove -o ./example
./example
```

The program exits with status 42. From a compiler checkout, use
`build/wovec` if `wovec` is outside your PATH.

### Program entry

The entry module's `main` takes a `(shared-array u8)` and a
`(shared-array ArgSpan)` and returns `i32`. The span record can have any
name and must contain `(start i32)` followed by `(end i32)`. The byte view
concatenates the user arguments without terminators. Each span is
half-open and indexes that view. The executable name is omitted. Empty
arguments have equal endpoints, and no user arguments give two empty views.

## Modules, imports, and metadata

Select names from another Wove file explicitly. A relative path resolves
from the importing file.

```lisp
(use wove-file "./scanner.wove"
    (only scan (as scan_next next) (type Token) (value alphabet)))
```

A bare name selects a function or immutable value, preferring the
function if both use the same name. `(type (as Token ScanToken))` and
`(value (as alphabet digits))` rename explicit type and value selections.
An empty `(only)` visits the provider without binding a name. Aliases
retain the original declaration's identity and storage.

A metadata entry uses a colon-prefixed tag such as `(:doc "text")`.
Module and module-level `let` metadata follows the name as a contiguous
prefix. Function metadata can appear anywhere after the name among its
tagged fields. On other ordinary forms, use a contiguous prefix directly
after the tag. A C bridge's prefix follows its header.

```lisp
(:expect (tests "examples/scan.wove") (issues "gh:owner/repo:42"))
```

The `:expect` groups are optional. Test paths are relative to `tests/`,
and issues can also use full URLs. Use `(:pragma (cfg (os "linux")))`
for a platform-selected declaration. Multiple `cfg` predicates must all
pass. `(not predicate)` negates a selection. A `(decls ...)` group can
apply selection to several declarations. A `disabled` group keeps its
metadata and source forms while the compiler skips its contents.

A function marked `(:pragma (test))` uses `(params)` and `(returns void)`.
Test-mode selection admits test functions and `(cfg test)` helpers. Ordinary
builds exclude them.

## Literals

Booleans are `(bool true)` and `(bool false)`. Give integer values an exact
type with a suffix (`-3i32`, `1_000u32`, `0xabcd_ef00u64`, `0b1100_0011u8`)
or a type head (`(i32 -3)`). Place a separator between digits of the
selected radix. Signed hexadecimal and binary values put the minus before
the radix prefix and must fit the named type.

A single-quoted character such as `','` or `'\n'` is an `i32` byte value.
Use printable ASCII or an escape. The escapes are `\0`, `\a`, `\b`,
`\t`, `\n`, `\v`, `\f`, `\r`, `\'`, `\\`, and `\xNN` with two
hexadecimal digits. Use `(u8 '\n')` or `(int.convert u8 '\n')` to obtain
a `u8`.

Double-quoted strings use JSON escapes. A single backtick delimits a raw,
possibly multiline string. Inside a raw string, double a backtick to
include it. Other characters are literal, and CRLF and CR become LF in
the decoded value.

## Integers and booleans

| Width | Signed | Unsigned |
| --- | --- | --- |
| 8 bits | `i8` | `u8` |
| 16 bits | `i16` | `u16` |
| 32 bits | `i32` | `u32` |
| 64 bits | `i64` | `u64` |
| 128 bits | `i128` | `u128` |

The 128-bit types use the native `__int128` and `unsigned __int128` ABI.
Wove checks `__SIZEOF_INT128__` for the selected Clang target and reports
an unavailable width at the Wove source location.

Typed numeric operators use `type.operation`. In `(i32.add left right)`,
`i32` specifies the exact operands and result. The same integer operators
are available at every listed width.

| Operation | Words |
| --- | --- |
| Wrapping arithmetic | `add`, `sub`, `mul`, `inc`, `dec` |
| Checked arithmetic | `add-checked`, `sub-checked`, `mul-checked` |
| Division and remainder | `div`, `rem` |
| Recoverable arithmetic | `try_add`, `try_sub`, `try_mul`, `try_div`, `try_rem` |
| Comparisons | `eq`, `lt`, `le`, `gt`, `ge` |
| Compare with zero | `eqz`, `ltz`, `lez`, `gtz`, `gez` |
| Negated comparisons | Prefix any predicate with `not-`, as in `u32.not-eq` or `i32.not-ltz` |
| Bitwise operations | `and`, `or`, `xor`, `not` |
| Shifts and rotations | `shl`, `shr`, `rotl`, `rotr` |
| Bit counts | `clz`, `ctz`, `popcnt` |
| Range and checked conversion | `T.fits`, `T.from-S`, `int.convert` |
| Endian byte conversions | `to-le-bytes`, `to-be-bytes`, `from-le-bytes`, `from-be-bytes` |
| Boolean logic | `bool.and`, `bool.or`, `bool.not` |

- Integer arithmetic evaluates each written operand once, from left to right.
  Their types and the result type must match the type named in the tag.
  `(T.inc value)` and `(T.dec value)` add or subtract one with wrapping.
  They return the new value and leave the operand unchanged.
  Checked arithmetic traps if the mathematical result is outside that type's
  range, including unsigned subtraction below zero. The diagnostic points
  to the operation head.
- Comparisons return `bool` and evaluate each written operand once, from
  left to right. A zero predicate compares with zero of the selected type,
  including when the result is fixed by unsigned ordering. `int.eq` and
  `int.not-eq` also accept a selected native integer alias as their exact
  type. `bool.and` and `bool.or` short-circuit.
- Bitwise `and`, `or`, `xor`, and `not` take and produce the exact integer
  type named in the tag. `not` takes one operand. The other three take two.
  Operands are evaluated once, from left to right.
- A shift preserves the word type and takes an exact `i32` count. It evaluates
  the word and then the count, once each. Counts outside `[0, width)` trap
  at the full shift span. Left shifts keep the low bits of the declared width.
  Right shifts extend the sign for signed integers and fill with zeros for
  unsigned integers.
- `(T.rotl word count)` and `(T.rotr word count)` rotate an exact `T` word's
  bits left or right and return the same type. The word and an exact `i32`
  count are evaluated once in that order. Zero leaves the word unchanged.
  Counts outside `[0, width)` trap at the count operand's source span.
- `(T.clz word)`, `(T.ctz word)`, and `(T.popcnt word)` count leading
  zeros, trailing zeros, and set bits, respectively. Each evaluates one
  exact `T` operand and returns `i32`. Counts use the declared-width bit
  pattern, including the two's-complement bits of signed operands. For
  zero, `clz` and `ctz` return the width in bits and `popcnt` returns zero.
- Division truncates toward zero, and a nonzero remainder has the sign of
  the dividend. Both operations trap on zero divisors. Dividing a signed
  minimum by `-1` traps, and its remainder by `-1` is zero. Traps point to
  the full operation span.
- `(int.convert Type value)` converts a fitting integer, including native
  integer aliases, and traps on an out-of-range value. A typed checked form,
  such as `(u8.from-u32 value)`, also requires the operand to have the exact
  source type named in the tag.
- `(T.fits value)` evaluates the input once and returns whether the integer
  fits the built-in integer type `T`.
- `(T.try_add left right (borrow-mut place))` and the corresponding
  `try_sub`, `try_mul`, `try_div`, and `try_rem` forms evaluate exact
  `T` values left to right, then borrow the writable `T` place. They
  return `true` and store a representable result. Overflow or a zero
  divisor returns `false` and preserves the destination. Dividing a signed
  minimum by `-1` fails. Its remainder by `-1` succeeds with zero.
- Integer byte conversions use `T.to-le-bytes`, `T.to-be-bytes`,
  `T.from-le-bytes`, and `T.from-be-bytes`. They use fixed-width
  two's-complement encodings for signed types. Encoding produces a fixed byte
  array, and decoding requires a shared byte view of exactly that width.
  Both 128-bit types use 16 bytes.

The [128-bit decimal writers](stage-2/text/decimal_128.wove) preserve the
full signed and unsigned values in a caller-supplied byte view.

## Floating point

Wove uses native C `_Float16`, `float`, `double`, and `long double` for
`f16`, `f32`, `f64`, and `f80` in locals, function signatures, struct fields,
and C bridges. `f128` uses the Clang target's native binary128 type. Write a
typed decimal as `(f16 -2.5)` or `-2.5f16`. A decimal needs digits before the
point and in each written fraction or exponent. Clang rounds the original
digits at the written width and preserves a negative sign on zero.

Native `cc` and `bin` check the format using the selected Clang and its target
options when the checked program uses `f16`, `f80`, or `f128`. The `f80`
format requires a 64-bit `long double` significand and uses the target's
native C size and alignment. For `f128`, the compiler uses `long double`
with a 113-bit significand if available and otherwise uses Clang's
`__float128` extension. Decimal literals use the matching C suffix. On
unsupported targets the compiler reports a source diagnostic before emitting C.

Use width-qualified `add`, `sub`, `mul`, `div`, and unary `neg` for native
floating arithmetic. The `eq`, `not-eq`, `lt`, `le`, `gt`, and `ge` operators
take two values of the named width and return `bool`. Eager operands evaluate
once from left to right.

Use `(f32.from-bits word)` and `(f32.to-bits value)` to copy the representation
between `f32` and `u32`. The matching `f16` and `f64` forms use `u16` and `u64`,
respectively. These operations retain signed zero and NaN payloads. For example, an STL coordinate read from
little-endian bytes can widen with `(f64.from-f32 (f32.from-bits (local word)))`.

The same `<destination>.from-<source>` syntax handles checked numeric conversions
involving floats, with integer operands and results through 64 bits.
Floating-to-integer conversions truncate toward zero and
trap at the conversion span if the result cannot fit, including for NaN and
infinity. `f32.from-f64` keeps native rounding, subnormals, and nonfinite
values, and traps on finite values outside the binary32 range. Conversions
to `f16` round at binary16 precision, preserve signed zero and nonfinite
values, and trap on finite values outside `[-65504, 65504]`. Conversions
from `f80` and `f128` check the original value before rounding directly
to the destination format or truncating to an integer. Both formats
represent all signed and unsigned integers of up to 64 bits exactly.

## Values and storage

| Form | Meaning |
| --- | --- |
| `(local name)` | Read a local, parameter, or visible immutable module binding. |
| `(call name args...)` | Call a visible function. Arguments evaluate left to right. |
| `(set (local name) value)` | Replace a mutable local with a value of its exact type. |
| `(set (struct.get place field) value)` | Replace an exact field through a mutable Wove record local or reference, including along a nested field path. |
| `(struct.new Type (field value) ...)` | Construct a record from typed field values in written order. |
| `(struct.get value field)` | Read a field with its declared type. |
| `(array.alloc T count)` | Allocate an owned, zeroed buffer with an `i32` element count. |
| `(move (local buffer))` | Transfer ownership of a local buffer or hand a present native owner to a consuming C call. |
| `(owner.is-null (local file))` | Test whether a native owner holds a missing handle. |

A module-level `let` can hold an immutable scalar, a string, or an inline
table. A quoted string gives a `(shared-array u8)` view whose length counts
the decoded bytes. Declare an inline table with `array.new` or `array.repeat`.
Its elements are exact scalar literals or constant constructors of a
named Wove record. `array.load` can read the table directly.

### Function values

Use an inline `(fn-type (params T...) (returns R))` on a Wove parameter.
The signature compares exact types in order. Parameter names on the
defining function have no effect.

```lisp
(fn increment (params (value i32)) (returns i32)
    (body (return (i32.inc (local value)))))

(fn apply (params (f (fn-type (params i32) (returns i32))) (value i32))
    (returns i32)
    (body (return (call (local f) (local value)))))

(fn example (params) (returns i32)
    (body (return (call apply (fn-ref increment) 41i32))))
```

`(fn-ref name)` takes a concrete Wove function visible in the current
module, including a selected import. Function values are copyable. Bind
one with `let` or `let-mut`, or pass it to another Wove function with the
same signature. `(call (local f) args...)` reads the target once, then
evaluates each argument once from left to right. A callable with a
`void` result uses the standalone call statement.

Stage 2 supports built-in scalar parameters and scalar or `void`
results in these signatures.

Use a module type name to reuse the same signature in multiple declarations.

```lisp
(type UnaryI32 (fn-type (params i32) (returns i32)))

(fn forward (params (operation UnaryI32) (value i32)) (returns i32)
    (body (return (call apply (local operation) (local value)))))
```

`UnaryI32` belongs to the module's type namespace. Select it from another
module with `(type UnaryI32)` in an import's `only` list. An alias is
transparent, so values can pass between alias-typed and inline parameters
with the same signature.

### Generics

Records and functions can declare explicit type parameters and constant
parameters of type `i32`, `bool`, or a fieldless enum.

```lisp
(struct Block
    (generic (type T) (const N i32))
    (fields (items (array T N))))

(struct.new (specialize Block u8 4i32)
    (items (array.new (array u8 4) 1u8 2u8 3u8 4u8)))
```

In a record template, a type parameter can name a copyable field or fixed
array element, and an `i32` constant parameter can set an extent at any
fixed array layer.
Apply the template explicitly in type positions and as the first operand of
`struct.new`. Each argument must match its declared kind. A specialization
is identified by its defining record and normalized arguments. Different
arguments define distinct types even when their layouts match.

Functions can also have explicit generic parameters. Place the
`generic` list before the ordinary signature fields and specialize the
function at its call site.

```lisp
(fn last (generic (const Width i32))
    (params (bytes (array u8 Width)))
    (returns u8)
    (body (return (array.load (local bytes) (i32.sub (local Width) 1i32)))))

(call (specialize last 4i32) (array.new (array u8 4) 1u8 2u8 3u8 4u8))
```

A type parameter supplies the exact types in the concrete signature and
body. An `i32` constant parameter can size an array, supply a nested
specialization argument, or be read with `local`. A `(const Enabled bool)`
parameter accepts `(bool true)` or `(bool false)`. A function can read it
with `(local Enabled)` or forward it as an argument to another
specialization.

```lisp
(enum Mode (variants (Fast) (Safe)))

(struct State (generic (const M Mode)) (fields (cursor i32)))

(fn choose (generic (const M Mode)) (params) (returns Mode)
    (body (return (local M))))

(call (specialize choose (enum.new Mode Fast)))
```

An enum constant parameter resolves its declared type in the template's
defining module. Its argument must name a variant of that exact enum, and
every variant in the enum must be fieldless. A function can read the value
with `(local M)` or pass `M` directly to another specialization.
Equal arguments to the same defining declaration share one specialization,
including when different import aliases name the same enum. Different
variants distinguish nominal record types even when their fields match.

### Records and ownership

A struct defines a nominal type. Copyable fields can hold scalars,
fixed arrays, and other records. `struct.new` supplies each field once.
Fields evaluate in the written order and must match their declared types.

`(shared-ref Type)` and `(mut-ref Type)` form record parameter types.
Use `borrow-shared` or `borrow-mut` on a named record place to lend it for a call.
A `(mut-ref T)` parameter can also borrow a writable place of an exact
built-in scalar type. In the callee, `(local name)` reads the scalar and
`(set (local name) value)` updates the caller's storage.

A function with `(returns void)` can use `(return)` or finish its body.
Call it as a statement. A value-returning function must return its declared
type on every path. `let` creates a binding and `let-mut` makes it writable.
A local name comes into scope after its initializer.

A struct field can hold any supported `owned-array` type, including a
numeric array, an array of Wove records, or nested owned arrays.
Borrow a field through its original place with `borrow-shared` or
`borrow-mut`. The view keeps the field's exact element type at each level,
and an exclusive borrow requires a writable record place.

A struct can own another struct as a field in a finite by-value layout.
Ownership propagates through every enclosing struct. Borrow nested fields
through their original places using a chain of `struct.get` expressions.

Move a whole local record with `(move (local grid))` or an owning field
with `(move (struct.get (local grid) cells))`. A move can take a nested
struct or a leaf array. Sibling fields stay available. Moving or borrowing
an entire struct requires all its owning fields to be present.

On scope exit, remaining owners drop in reverse declaration order.
A struct drops its remaining owning fields in reverse field order,
including owners inside nested structs and arrays. A return evaluates
its result before cleanup.

An `(embed "./payload.bin")` initializer resolves relative to its defining
module and gives a fresh owner with the file's original bytes on each
evaluation. The compiled program carries the payload.

## Control flow

Statement `(if condition (then ...) (else ...))` takes a `bool` and runs
only the chosen block. Omit `else` to fall through on false.
`(while condition (body ...))` checks a `bool` before each iteration and
starts a fresh body scope each time.

A value `(if condition (then value) (else value))` requires one
expression of the same exact type in each arm. It evaluates only the
chosen expression. An arm can produce a fresh owner or move an existing one.

| Iteration | Form |
| --- | --- |
| Half-open range | `(for (bind i) (range start end) (body ...))` |
| Read array elements | `(for (bind item) (borrow-shared (local data)) (body ...))` |
| Write array elements | `(for (bind item) (borrow-mut (local data)) (body ...))` |
| Consume owned rows | `(for (bind item) (move (local rows)) (body ...))` |

Range bounds have the same type, `i32` or `u64`. They evaluate once, from
left to right. An array loop can use `(bind index item)` for a zero-based
`i32` index relative to its view. Shared iteration copies copyable elements.
When iterating an array of owned rows with `borrow-shared`, the item is a
shared view of the current populated row with its exact element type.
It lasts through the iteration body, and the loop holds its parent loan
until exit. An unfilled row traps when visited. Mutable iteration gives a
writable place for copyable elements. Consuming iteration transfers each
row to the loop body.

`(break)` and `(continue)` act on the innermost loop. They release owners
in the scopes they leave. Exiting consuming iteration also releases any
unvisited rows.

## Enums

Use `(enum Status (variants (Ok) (IoError) (Invalid)))` to
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
`(case _ ...)`. Bare decimal, hexadecimal, and binary patterns take the subject's
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
| `(borrow-shared (array.range (local data) start end))` | Lend a half-open range of a named owner or view. |
| `(borrow-mut (array.range (local data) start end))` | Lend a half-open range from a mutable owner or exclusive view. |

Indexes and range endpoints are exact `i32` values. The compiler checks
`0 <= start <= end <= length` for a range. The view counts and indexes
elements relative to its own bounds. Equal endpoints give an empty view.
A call loan lasts through later arguments and the call.

Growable arrays can use a named owner, a field of a named place, or an
indexed owning row as their receiver. Mutation requires writable storage
with no overlapping live loan.

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

An array of owned rows accepts
`(array.push (local rows) (move (local row)))`. A successful push
transfers the original owner to the vector. Removing a populated
row transfers it to the result, and removing an unfilled slot traps.
Truncation drops the owners in the removed suffix immediately.

`(array.push (array.load (local batches) index) (move (local row)))`
appends to a populated batch. An indexed receiver is checked before later
operands run, and each index evaluates once. The outer slot keeps the row
owner as its storage grows. Selecting an unfilled slot traps.

`array.alloc` supports numeric elements, Wove records, and nested owners.
For example, `(array.alloc (owned-array Digest) count)` creates an
`(owned-array (owned-array Digest))` with unfilled row slots. Fill a slot
with `array.store` and a fresh or moved owner of the exact row type.
Replacing a slot drops its previous row. Use
`(borrow-shared (array.load (local rows) index))` to read a populated row
through its exact shared view.

An owned array can store owning Wove structs with no borrowed fields.
`(array.alloc Entry count)` starts with unfilled slots. Use `array.store`
or `array.push` with a fresh `Entry` or an explicit `(move (local entry))`.
Replacing a populated slot drops its old record. Removing a populated
slot transfers its record out and shifts later slots. Truncation and
array cleanup release the owning fields of each remaining populated slot.

Use `(borrow-shared (array.load (local entries) index))` for a
`(shared-ref Entry)` call parameter or `borrow-mut` for a `(mut-ref Entry)`
parameter when the original array view is writable. Borrowing or removing
an unfilled slot traps. Indexed loans keep the source array live for the
call. An owning slot cannot be loaded by value or bound as an array
iteration item.

Fixed arrays keep their elements inline and copy by value. Use
`(array T extent)` for a builtin scalar, copyable Wove record, or another
fixed array. Each extent is a nonnegative decimal or an immutable module
integer. For example, `(array (array i32 3) 2)` holds two rows of three
`i32` values. The type includes the leaf type and every extent.

- `(array.new (array u8 2) (u8 17) (u8 25))` lists the elements.
- `(array.repeat (array u8 128) (u8 0))` evaluates one value and fills the array.
- `(array.load value index)` reads an element.
- `(array.store (local data) index value)` writes into a mutable local array.
- `(array.store (struct.get (local state) block) index value)` writes into
  an inline array field of a writable record.
- `(array.store (array.load (local grid) row) column value)` updates the
  selected row in the original writable grid. In a value expression,
  `(array.load (local grid) row)` returns a copy of the row.

A fixed integer array or record field can lend a matching `shared-array` or
`mut-array` view. Sibling field paths can lend independently. A whole-record
loan overlaps all of its fields. To borrow a range from a fixed array, first
bind a view of the whole array.

`(let view (borrow-shared (local data)))` binds a view for the enclosing
block. A shared loan permits source reads. An exclusive loan also excludes
reads through the source. Mutable views can write their underlying elements.
Reborrows follow the same rules for their immediate parent view.

A parameter can require an exact count, as in `(shared-array u8 4)`. An
argument whose length is unknown at compile time is checked before later
arguments run. The usual range bounds still apply.

Return a fresh owner from a call or transfer an existing owner with
`(return (move (local buffer)))`. A move ends the original binding's
ownership. Completed call borrows allow a move.

A Wove parameter such as `(params (job (owned-array u8)))` takes ownership
of an array of the exact element type. Pass a fresh owner or move a named
owner explicitly with `(move (local input))`. The callee owns the incoming
array for its function body and releases it at exit unless it transfers
ownership again.

## Joined workers

`(worker.spawn-join widen (move (local input)))` calls the named Wove
function `widen` on a fresh thread and waits for its result. The handler
takes one owned array and returns an owned array through a normal Wove
return. Its input and output element types may differ. Arrays that contain
borrowed or native data cannot cross the worker boundary. After a
successful join, the caller owns the returned array. Thread creation and
join failures report the source span of the worker expression. When
linking emitted C directly on a POSIX host, pass `-pthread`.

## Submitted workers

`worker.spawn` starts the same kind of named handler on a fresh thread and
returns a receipt without waiting. Bind it directly to a local. The caller
can start other jobs or do work before it joins.

```lisp
(let first (worker.spawn widen (move (local first_job))))
(let second (worker.spawn widen (move (local second_job))))
(let second_result (worker.join (local second)))
(let first_result (worker.join (local first)))
```

`worker.join` consumes the receipt and returns the exact owned array type
declared by the handler. Use `move` to give a receipt to another local.
Exiting its scope, including by early return, joins a live receipt and drops
the unclaimed result. Launch failures and failures during implicit cleanup
report the submission span. An explicit join failure reports its join span.
C code reached by multiple workers is responsible for synchronizing its shared
state.

## Worker receipt queues

`(worker.receipt-queue digest_batch)` creates an empty local queue for
receipts whose result type matches `digest_batch`. It starts no worker.
Use a mutable binding to enqueue and move each receipt explicitly.

```lisp
(let-mut pending (worker.receipt-queue digest_batch))
(let receipt (worker.spawn digest_batch (move (local batch))))
(worker.push (local pending) (move (local receipt)))
(let results (worker.join-all (local pending)))
```

`worker.join-all` consumes the queue and returns one owned result per
receipt in enqueue order, even when workers finish in a different order.
For a handler returning `(owned-array Digest)`, the result is
`(owned-array (owned-array Digest))`. An empty queue returns an empty
array of the same type. Moving a queue to a new local transfers its
pending receipts.

Exiting a queue's scope, including by early return, joins all uncollected
receipts and drops their results. If collection fails, the runtime
releases the collected results and retires the remaining receipts before
reporting the join span. An implicit cleanup failure reports the failing
worker's original submission span.

## Bounded worker pools

`(worker.open-pool threads capacity)` takes two positive `i32` values.
The pool starts a fixed number of reusable threads. Its capacity counts jobs
waiting to start and reserved slots together. Running jobs free their waiting
slots. A failed open produces an empty owner.

`(worker.try-reserve (local pool))` attempts to claim one slot without waiting.
`(worker.reserve-wait (local pool))` can wait for a queued job to start. It
returns an empty permit when admission is closed or all capacity is held by
unused permits. Test a pool or permit for an empty owner with `owner.is-null`.

```lisp
(let pool (worker.open-pool 4i32 8i32))
(let permit (worker.reserve-wait (local pool)))
(if (owner.is-null (local permit))
    (then (return 1i32)))
(let receipt (worker.spawn-on digest_batch (move (local permit)) (move (local batch))))
(let results (worker.join (local receipt)))
```

Open, reserve, and submit directly into local bindings. Reserving leaves the
job with its caller. Submission moves a valid permit before evaluating the job
and returns a receipt of the same type as `worker.spawn` for that handler.
Join a pooled receipt directly or move it into a matching receipt queue.
Leaving the scope of an unused permit releases its slot.

`(worker.try-close (local pool))` drains accepted jobs and joins the threads.
It returns false while any permit is outstanding and leaves admission open.
Leaving the scope of the pool closes admission. Existing permits can still
submit or release their slots, and resolving the last permit then drains
the pool. Receipts can be collected after either kind of closure.

## C interop

Use a C header's bundled declarations or provide Wove signatures in
`(bridge c-header "<header.h>" ...)`. A project header can use a quoted
path such as `"./project.h"`. Select functions, types, or values explicitly,
just as with a Wove module.

```lisp
(bridge c-header "<stdlib.h>"
    (fn absolute (params (value i32)) (returns i32) (c-name "abs")))
(use c-header "<stdlib.h>" (only absolute))
```

A bridge function has `params` and `returns` with no body. `c-name`
overrides the native symbol. Arguments are checked against the Wove
signature and evaluated once from left to right. The C header verifies
the native signature. A project bridge adds to a bundled header and can
override an individual name. `(:pragma (bridge_replace))` selects only
the project declarations. `(:pragma (c_optional))` requires a selected
function to be present in the target header.

Use `(type Size (c-type "size_t"))` for a native integer alias and
`(struct Stat (c-type "struct stat") (fields (st_size Offset)))` for a
native record view. `(struct.new Stat)` gives zero-initialized native
storage. Pass `shared-ref` or `mut-ref` arguments with an explicit borrow.
Bridge `const` exposes an integer constant, `let` reads a C value, and
`let-mut` also accepts a `set` on the imported C lvalue.

Attach native contracts to the relevant declaration with `:pragma`.

| Directive | Contract |
| --- | --- |
| `(c_macro)` | Bind a C macro with an exact signature or value type. Macro arguments evaluate once. |
| `(c_cstr parameter ...)` | Copy byte views into NUL-terminated C strings and reject embedded NUL. Mutable views copy back their original length. |
| `(c_cstr_raw parameter ...)` | Copy all bytes, including embedded NUL, before adding a terminator. |
| `(c_cstrs parameter ...)` | Copy shared byte rows into a null-terminated array of C-string pointers. Reject embedded NUL. |
| `(c_bytes data count)` or `(c_bytes data size count)` | Borrow bytes after checking the requested count or product fits the view. |
| `(c_array data count)` | Borrow an integer slice after checking its element count. Use a literal or a named integer parameter. |
| `(c_variadic)` | Call a C variadic function with just its declared fixed arguments. |
| `(c_variadic first_tail_parameter)` | Declare one typed call shape. Clang applies native promotions to the variadic arguments. |
| `(c_variadic ...)` | Keep all declared parameters fixed and accept zero or more scalar arguments after them. |
| `(c_null parameter)` | Require a literal `null` for the named byte-view slot. |
| `(c_nullable parameter)` | Accept either a mutable scalar reference or literal `null` for the supported slot. |

A fixed C argument in `params` supplies a native operand at that position.
Use `(const command i32 F_SETFL)` for a header constant,
`(const length i32 -1)` for a checked signed literal, or
`(const tail (c-type "const char **") null)` for a null pointer with the
exact C type. A fixed `i32` literal may also carry its `i32` suffix or
use the `(i32 ...)` form. Clang verifies the signature against the
included header. Pass only the ordinary parameters at Wove call sites.

An open C tail accepts built-in scalars and native integer aliases. The
fixed arguments still follow their declared types and contracts, including
C-string conversion for the bundled `printf` and `fprintf` formats.
Wove evaluates every supplied argument once, from left to right, and
passes each captured tail value with its exact C type. Clang applies the
target's default promotions, including promoting `f32` to C `double`
and applying integer promotions to narrow values. For formatted calls,
the format must agree with the promoted types. The bundled `snprintf`
checks that its capacity fits the mutable output view before entering C.

A pointer-backed resource uses a destructor contract.

```lisp
(type (:pragma (c_owned (drop fclose))) File (c-type "FILE *"))
```

A `File` result owns the native handle. Cleanup calls the destructor once
for each present handle. Test presence with `(owner.is-null (local file))`.
An ordinary Wove function can declare `(returns File)` after importing
the native type. Return a fresh handle call or explicitly transfer a
local with `(return (move (local file)))`. The caller owns the result,
including the possibility of a null handle. Renamed type imports keep
the same defining owner type.
An exact `(shared-ref File)` parameter borrows a present handle. A consuming
parameter of type `File` takes `(move (local file))` and makes the C callee
responsible for cleanup, including when the call reports an error.
`(let stderr (shared-ref File))` can expose a read-only library-owned handle.

Use `(c_init output)` on a mutable owner-reference parameter for C
initializers. For pointer handles, declare a mutable local with
`(native.empty Database)`, then lend its empty slot to C. Any nonnull output
belongs to the local even when the call fails. For contracts declared
with `(c_owned (drop destroy) storage)`, use `(native.alloc Actions)`.
A successful initializer activates the object. Cleanup destroys active
objects and releases their storage.

`(c_snapshot cstr)` copies a returned C string into
`(optional (owned-array u8))`. Add `(drop free)` to release the original,
or `(field name)` to copy a bounded inline character array. A null
result stays absent. After testing presence, use
`(move (optional.unwrap (local result)))` to extract the byte owner.
`(c_snapshot cstrs)` on a read-only native table copies it into an
`(owned-array (owned-array u8))`.

<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 2 source scanner, tree reader, and renderer

Call `source_diagnostic` once on the shared byte slice before tokenization.
The admission check covers the entire view, including bytes in quoted metadata.
Pass the same bytes to `scan_token` after a successful check.

`scan_token` reads one token from a shared byte slice. Start with an `i32` cursor
of zero and use each token's `end` as the next cursor. A cursor from zero through
the slice length is valid. The runtime rejects cursors outside that range.

Each `Token` carries a `u32` kind and an `i32` half-open span, `[start, end)`,
relative to the supplied slice. The scanner uses the seed's whitespace set
for both skipping separators and ending bare atoms. Separators consume their
UTF-8 byte widths. A bare atom also stops at a parenthesis, semicolon,
apostrophe, double quote, or backtick. Bare atoms have kind 1. Opening and closing
parentheses use kinds 40 and 41. An unquoted semicolon produces a one-byte lexical
error of kind 3. End of input has kind 0 and both offsets equal to the slice
length.

A double-quoted string is kind 2. Its raw span includes both quotes and
preserves the original spelling of JSON short escapes and `\u` escapes with
four hexadecimal digits. A raw byte below `0x20`, an invalid escape, or a
missing quote gives error kind 3. The error starts at the opening quote and
ends just after the offending byte or at the view end on truncation.

String and character tokens can contain semicolons.

A raw backtick string also has kind 2. A doubled backtick belongs to the
string and a single backtick closes it. Use the opening byte to distinguish
the string spellings. The span keeps both delimiters, all doubled backticks,
and the original physical line endings. The scanner preserves literal
backslashes, quotes, and semicolons inside backticks. If the view ends
before a closing backtick, the error span starts at the opener and ends at
the view boundary.

A single-quoted character has kind 4. Its span includes both quotes. The
scanner reports an invalid payload or escape at the literal and stops an
unterminated character at the end of its line or view.

The `error` field of a token is zero on success. Error tokens use kind 3
and carry a primary span with a numeric failure code.

| Code | Failure |
| --- | --- |
| 1 | Unexpected delimiter |
| 2 | Unterminated literal |
| 3 | Invalid or incomplete escape |
| 4 | Raw control byte in a string |
| 5 | Invalid UTF-8 |
| 25 | Character payload does not represent one byte |

`source_diagnostic` reports code 5 with a one-byte span at the first
invalid sequence and code 0 for a valid view. `admit_source` exposes the
same check as a scalar offset, with `-1` on success. A stream stops at
admission failure, its first lexical error, or one EOF token.

`source_position` maps the start of a token or diagnostic to one-based
line and column. Pass the parent source, the view's starting byte offset
within it, and the view-relative span start. LF, CR, and CRLF each count
as one line break. Columns count Unicode scalar values, including tabs.
Each malformed byte in an earlier part of the parent uses one column.

The first-byte helpers `source_offset` and `source_kind` find the first
significant byte from cursor zero. `source_kind` returns 0 at EOF, 40 or 41
for a parenthesis, and 1 for every other leading byte.

`check_source_form` validates exactly one top-level tagged form. Each opener
requires a bare tag as its first token. The following children can be bare
atoms, quoted tokens, or nested forms.

The result gives the real root's span, including its closing parenthesis,
and the number of rows needed for the source tree. Count one synthetic source
root and one row for each opener, bare atom, or quoted atom.

A failed check returns zero rows and the original diagnostic span. Lexer and
admission errors retain their original codes. Structural checks use the following codes.

| Code | Failure | Primary span |
| --- | --- | --- |
| 6 | Expected a root form | The first token, or EOF for empty or whitespace-only input |
| 7 | Expected a bare tag | The significant token that followed the opener |
| 8 | Unexpected closer | The closer without a matching opener |
| 9 | Unclosed form | The innermost unmatched opener |
| 10 | Extra input after the root | The first extra significant token |

`build_source_tree` takes the same admitted source view, its successful
`FormCheck`, and a writable `(mut-array SourceNode)`. It checks capacity
before writing. On success it fills exactly the counted rows in source
order and returns the original check. Short storage returns code 11
with the real root's span.

Row 0 is the synthetic source root and covers the entire view. Node kinds
are 0 for that root, 40 for a parenthesized form, 1 for a bare atom, 2 for
a string, and 4 for a character. Each row carries a half-open byte span and `parent`,
`first_child`, and `next_sibling` indices. An absent link is `-1`. A form
spans both parentheses and has its tag as its first child.

After building the tree, call `check_source_metadata` with the same source
view, successful result, and shared node slice. Form heads use
`[A-Za-z_][A-Za-z0-9_.-]*`. A leading colon marks a metadata entry. For a
module, function, or module-level `let` declaration, the bare name directly
follows the tag. Function declarations allow metadata anywhere after the
name, alongside their tagged fields. A Wove definition requires exactly
one `params`, one `returns`, and one `body` in any order. Spell `(params)`
when the function takes no arguments. Selection pragmas apply to the
owning function wherever they appear among its fields.

```lisp
(fn succeed (returns i32) (params) (body (return 0i32))
  (:doc "Return success."))
```

Modules and module-level bindings put a contiguous metadata prefix after
the name. C bridges put theirs after the header. Other forms use the prefix
immediately after the tag. Name slots follow declaration ancestry through
declaration groups and supported C bridges. An opaque payload can contain
the same tags as ordinary data. Each entry keeps its order, parent link,
spelling, and original byte span.

| Code | Failure | Primary span |
| --- | --- | --- |
| 12 | Invalid head spelling | The head atom |
| 13 | Misplaced metadata entry | The colon-prefixed head atom |
| 14 | Retired `with-meta` or `meta` form | The head atom |
| 19 | Missing or invalid module name | The module form or invalid name operand |
| 22 | Missing or invalid function name | The function form or invalid name operand |

`render_source_tree` takes the original byte view, its built and validated
shared node slice, and a writable byte slice. It walks the tree links in
source order and copies each token's raw bytes, including parentheses,
metadata entries, quoted escapes, and physical line endings inside raw
strings. It adds one space between tokens except immediately inside
parentheses. Layout around the outermost tokens is discarded.

The result contains the number of bytes written and an error code. Code 0
means the emitted prefix contains the entire source. Code 16 means the next
token and its separator could not fit. In either case the prefix ends on a
token boundary and the remaining output bytes retain their original values.

Use `decode_string` from [the string decoder](source/strings.wove) with the
original admitted byte view, a checked kind-2 token, and a writable byte slice.
It writes the string value as UTF-8 without changing the source. Double-quoted
values decode JSON escapes and combine a high and low surrogate into one
scalar. Backtick values collapse doubled backticks and normalize CRLF and lone
CR to LF.

The result gives the written byte count and a view-relative span. On success
the span covers the original token. A short output view returns code 16 and
the source span of the next scalar. An unpaired surrogate returns code 17 and
the span of its six-byte escape. In both cases the written prefix ends at a
scalar boundary and the remaining output retains its previous bytes.

The [module reader](source/module.wove) works from the original bytes and a
checked source tree. `form_parts` returns the head, optional module or
function name, metadata range, and first ordinary field. Empty views use
`-1`. Walk metadata siblings up to the first ordinary field and ordinary
fields to the end. All indices refer to existing tree rows.

`read_module` reads the name slot and writes the original indices of selected
declarations into a caller-provided record slice.
The caller chooses native, bootstrap, or test mode. `(cfg bootstrap)` selects
only in bootstrap mode. `(cfg test)` and a `(test)` directive select only in
test mode. The `not` predicate inverts either flag. The same rules apply to
the module, its groups, and its declarations. The reader flattens active
`decls` in source order and retains each declaration's direct test marker.
An excluded module succeeds with its name and zero declarations. Unknown metadata and
unknown directive payloads stay opaque. Later passes resolve imports and
interpret declarations.

The result includes the module name index, whether the module is active,
the number of written declarations, and a view-relative source span. On
success the span covers the module. A failure can retain indices written
before the error. Earlier tree errors keep their original spans and codes.

| Code | Failure | Primary span |
| --- | --- | --- |
| 16 | Short declaration output view | The next selected declaration |
| 18 | Expected a module root | The root head |
| 19 | Missing or invalid module name | The module if the name is absent, otherwise its first ordinary operand |
| 20 | Expected a declaration form | The offending ordinary operand |
| 21 | Invalid selection directive | The offending directive or predicate |

A `disabled` group can appear among module declarations, block statements,
or bridge members. It can hold a multiline `:doc` alone or retain forms
alongside its metadata. The checked source tree and renderer preserve the
group in place. Semantic traversal skips the entire group before selection,
name lookup, type checking, C emission, and caller inspection.

The [C emitter](emit/c.wove) consumes a checked tree, its module result,
and the selected declaration indices. A value-returning function must
return its declared type on every path. Helpers can accept and return `i8`,
`u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64`, `bool`, `f32`, and `f64`. Expressions support exact-type
literals, binding reads, and direct calls. A local declared with `let` or
`let-mut` takes its scalar type from its initializer and comes into scope
afterwards. A `set`
requires an existing `let-mut` binding and a value of that binding's exact
type. Forward calls use checked prototypes, and nested calls evaluate from
left to right.

Stage 2 integer atoms such as `0x80u8`, `-0x80i8`, and
`0xffffffffffffffffu64` use an exact integer suffix. Uppercase `0X` and
uppercase hexadecimal digits also work. Signed magnitudes must fit the
named type, and unsigned hexadecimal spellings have no leading minus.
The form `(u8 0x80)` supplies the type before the hexadecimal magnitude.

A single-quoted character such as `','` or `'\n'` is an `i32` byte value.
Use one printable ASCII character or one escape. The supported escapes
are `\0`, `\a`, `\b`, `\t`, `\n`, `\v`, `\f`, `\r`, `\'`, `\\`, and
`\xNN` with exactly two hexadecimal digits. Use `(u8 '\n')` or
`(int.convert u8 '\n')` for an explicit byte conversion. The Python seed
uses decimal integer spellings to build the compiler.

An immutable module binding takes a typed numeric literal, a character, a quoted
string, or a fixed-array initializer. Its name precedes metadata.

```lisp
(let lowest_digit (i8 -1))
(let alphabet "0123456789abcdef")
(let weights (array.new (array u32 2) 17u32 25u32))
```

Functions use `(local lowest_digit)` to read the scalar after declaring
or selecting the binding. A local or parameter with the same name takes
precedence. A quoted string is a shared `u8` view over its decoded
bytes. Generated C puts a referenced nonempty byte table in a single read-only array
with no added terminator. The ordinary `array.len`, `array.load`, and
`borrow-shared` forms work with the static view.

A module table declares its exact element type and extent with `(array T N)`.
Use `array.new` for exactly `N` elements or `array.repeat` for one repeated
element. Elements use scalar literals or constant `struct.new` values of a
visible Wove record type. Record fields can themselves use constant
constructors. The compiler checks every selected initializer, including
bindings with no uses. A referenced table gets one `static const` C
definition. A repeated initializer uses one C range designator, accepted
by Clang and GCC, so the initializer text stays compact as the extent
grows. `array.load` reads the original table at a checked `i32` index
and copies only the selected element. Stage 1 keeps its integer and
quoted-byte module initializer subset.

An enum declares one or more variants. Every value keeps its nominal
enum type through locals, assignments, parameters, and returns. A type alias
selected from another module keeps the defining enum and its variant names.

```lisp
(enum Status (variants (Ok) (IoError) (Invalid)))

(let status (enum.new Status Ok))
(match (local status)
    (case Ok (return 0i32))
    (case IoError (return 1i32))
    (case Invalid (return 3i32)))
```

The match evaluates its subject once and runs one case in its own scope.
An enum match must cover each variant or end in a `(case _ ...)` fallback
for uncovered variants. A declared variant named `_` keeps its exact
case spelling. A continuing statement case exits the match
after cleaning up its locals. A return exits the function and releases
its active owners. When every case returns, the match satisfies the
enclosing return requirement. Missing coverage reports code 78 at the
match. Unknown or duplicate variants point at the case name.

A variant can declare one payload, as in
`(enum Parsed (variants (Record Request) (Invalid)))`. Write
`(enum.new Parsed Record value)` with the exact `Request` type. A case
for that variant starts with `(case Record (bind request) ...)`. A copyable
payload is copied into its immutable arm-local name. `(bind _)` discards it. The compiler emits these enums as a tag and a union and reads
the selected member from the captured subject. Fieldless enums retain
their ordinal representation.

A variant can own a byte array or a named owning Wove value. Supply a
fresh value or an explicit move to `enum.new`. A statement match
consumes a fresh enum result or `(move (local input))` and transfers the
active payload to its case binding. A bound payload follows local
ownership rules, including transfers and cleanup on early returns.
A discard or wildcard keeps any active ownership until its arm exits.

A match can also dispatch on a built-in integer. A bare decimal or hex
pattern takes the subject type. A suffix or literal form names the exact
type, as in `0u32` or `(u32 0)`. Integer matches require a final
`(case _ ...)` for unmatched values. Different spellings of the same
integer count as duplicate patterns.

In a value position, each case provides one expression after its
pattern and any payload binding. Every expression must yield the same
exact copyable type. Only the selected expression runs.

```lisp
(enum Instruction (variants (Push i64) (Add) (Drop)))

(return (match (local opcode)
    (case 0u32 (enum.new Instruction Push (local immediate)))
    (case 1u32 (enum.new Instruction Add))
    (case _ (enum.new Instruction Drop))))
```

A helper with `(returns void)` can use `(return)` or finish at the end
of its body. Invoke it with a standalone `(call ...)` statement. The
call checks its arguments, and either exit releases the helper's live
local owners.

An executable's `main` takes a `(shared-array u8)` and a
`(shared-array ArgSpan)` and returns `i32`. The span record has `start` and
`end` fields of type `i32` in that order. A type alias retains the defining
record identity for calls to the [typed argv parser](cli/argv.wove).
Ordinary zero-argument functions use `(params)`.

Both compilers emit a host wrapper with `int main(int argc, char **argv)`. It skips the
executable name and copies the original argument bytes without their C
terminators. Each user argument has a half-open span in source order.
The first row is index zero. Empty arguments have equal endpoints, and
zero user arguments supply two empty views. Wove borrows both views for
the entry call. The wrapper releases the backing storage on return.

A C bridge can expose fields of a native record with
`(struct Stat (c-type "struct stat") (fields (st_size Offset)))`.
Select its type and the relevant function with `use c-header`.
`(let-mut info (struct.new Stat))` zero-initializes storage of the
header's complete type with its native alignment. A `(mut-ref Stat)`
C parameter takes `(borrow-mut (local info))` for the duration of the
call. A `(shared-ref Stat)` takes a shared borrow and passes a const
pointer. Subsequent `(struct.get (local info) st_size)` reads the
native field with its declared Wove type. The C header verifies
the field spelling and type, including the platform-specific
timestamp members in the bundled Stat views.

A module-local `(struct Token (fields (kind u32) (start i32)))` defines
a named, copyable record. Fields can hold scalar values, fixed arrays, or
other Wove records. Named field types resolve in the defining module,
including forward declarations and explicitly imported types. Inline
record layouts must be acyclic.
`(struct.new Token (kind (u32 40)) (start (i32 2)))` evaluates each
initializer exactly once in the order written. Supply every field once
with its declared type. A local stores the result by value, and
`(struct.get (local tok) start)` reads a field with its exact type. Chain
`struct.get` forms to reach nested fields.

Use `(set (struct.get (local tok) start) (i32 8))` to assign a field of a
`let-mut` local. The same form accepts a nested field path and writes
through to the original local. The right-hand value must have the
field's exact type. Copies made by binding, calls, or returns retain
their own fields.

A Wove function can accept `(shared-ref State)` or `(mut-ref State)` in a
parameter. The caller supplies `(borrow-shared (local state))` or
`(borrow-mut (local state))` with the same defining record type. Shared
loans read fields of the original record, and mutable loans let `set`
write through field paths. A mutable loan starts at a `let-mut` local
or an existing mutable reference. Reborrow a reference parameter with
the same explicit forms to pass it to another call.

A record with a `(shared-array u8)` field is scoped to its local declaration.
Initialize each shared field with an explicit `borrow-shared` of its
source. The loan lasts through the record's block and allows source reads.
Use a shared or mutable record reference to pass it to a Wove helper.
A mutable reference can advance a scalar cursor field while the byte
descriptor stays fixed. Array reads and shared reborrows of the descriptor
use the original backing storage. The record stays in its defining binding.

A field such as `(cells (owned-array u8))` makes its record an owner.
Initialize the field with a fresh array or `(move (local cells))`. Move
an entire local record with `(move (local grid))`, or transfer a single
field with `(move (struct.get (local grid) cells))`. After a field move,
sibling fields remain available. A whole-record move or loan requires all
owning fields to remain present. Scope cleanup drops the remaining owning
fields in reverse declaration order.

Pass `(borrow-shared (struct.get (local grid) cells))` to lend that array
to a helper. A mutable field loan requires a mutable local or reference.
A Wove helper can return the record by moving a local or producing a fresh
record.

For built-in scalars, a `(mut-ref T)` parameter receives an exclusive call
loan of a mutable place of exactly `T`. In the callee, `(local name)` reads
the scalar and `(set (local name) value)` writes the exact type back to that
place. `(borrow-mut (local name))` forwards the same storage to another call.

A borrow can name a static field path such as
`(borrow-mut (struct.get (local state) pending))` to lend a nested record
or writable scalar field. A fixed integer array field can lend a slice of
the same element type. The path can start at a local or a record
parameter, and a mutable loan needs a `let-mut` local or a mutable
reference parameter.

A loan lasts through later argument evaluation and the call. Mutable
loans exclude other accesses to the same field, its descendants, or
its enclosing records. Shared loans allow other shared access to those
places. Disjoint siblings can be borrowed together. An earlier by-value
argument keeps the value it had when evaluated.

Functions can take and return locally declared or explicitly imported records.
Calls pass these values by copy, and a caller retains its original local.
An argument or return must match the defining record identity, so two
types with the same fields remain distinct. Record layout declarations
can follow the signatures that refer to them.

`(array element extent)` describes a fixed array of a builtin scalar or an
ordinary copyable Wove record and a nonnegative `i32` extent. Use a bare decimal size or the
name of a local or selectively imported immutable module integer. The
defining initializer must be a literal that fits the extent range. Both
the element type and resolved size are part of
the type. Use `(array.new (array u8 2) (u8 17) (u8 25))` for a typed
list and `(array.repeat (array u8 128) (u8 0))` to fill a fixed block.
Lists evaluate their elements left to right. Repetition evaluates its
initializer once, including at extent zero. Repeating a record copies
that value into each inline slot. Aliases of the element record share
one layout for each resolved extent.

A fixed array keeps its elements inline and can be a record field, a
function parameter, or a return value. Binding, passing, and returning
an array or a record containing one copy the value. `(array.load value index)`
reads an element and `(array.store (local data) index value)` writes to
a `let-mut` local. An indexed store can also use a field place such as
`(array.store (struct.get (local state) block) index byte)`. The field
must belong to a mutable local or a mutable reference. The store holds
that field exclusively during evaluation of the index and value.
Disjoint sibling fields remain accessible. Both array operations take
an exact `i32` index and trap at the access span when it is out of bounds.

A fixed integer array local can lend all its elements to a function whose
formal is a `shared-array` or `mut-array` of the same element type. Supported
element types are `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, and `u64`.
For example, `(borrow-shared (local words))` supplies a matching
`(shared-array u64)` formal. A mutable loan uses
`(borrow-mut (local words))` and requires a `let-mut` local. The
descriptor carries the original backing storage and declared extent,
including zero. Loans last through the call and follow the same
exclusivity rules as other slice arguments.

In the callee, `array.len` counts elements and `array.load` returns
the declared element type. `array.store` takes an exact value of that type
and writes into the original fixed array through a mutable slice.
A checked load or store reports an invalid `i32` index at the original
operation span.

`(bool true)` and `(bool false)` are scalar literals.
`i32.eq` and `i32.lt` compare signed operands. `u8.eq`, `u8.lt`,
`u32.eq`, and `u32.lt` compare unsigned operands. Each comparison
requires operands of its named scalar type and returns `bool`.
`(int.eq Type left right)` also compares two integers of the exact
named type, including native aliases such as `Device` and `Inode`.
`i32.add.wrapping`, `i32.sub.wrapping`, and `i32.mul.wrapping` take `i32` operands and
return their signed 32-bit result modulo 2^32. Each binary operand is
evaluated once, left to right. Emitted C widens the arithmetic,
reduces it to 32 bits as unsigned, and produces an in-range signed result.

`(i32.div dividend divisor)` truncates the quotient toward zero.
`(i32.rem dividend divisor)` returns the signed remainder. Both take
two exact `i32` operands, evaluated once from left to right, and return
`i32`. A zero divisor traps at the original operation span. Dividing
`INT32_MIN` by `-1` also traps for overflow. The corresponding
remainder is zero.

`u32.add.wrapping` and `u32.sub.wrapping` take exact `u32` operands
and yield `u32` results modulo 2^32. Each operand runs once, left to
right. Emitted C widens the operands to unsigned 64-bit arithmetic and
keeps the low 32 bits.

Native `i16` and `u16` use C `int16_t` and `uint16_t` in signatures,
fields, and storage. Their `.eq` and `.lt` compare exact operands. Their
`.add.wrapping` and `.sub.wrapping` keep the low 16 bits. For decimal
output, `i32.from.i16` and `u32.from.u16` widen exactly to the types used
by the shared decimal writers.

Native `u64` values use `uint64_t` in emitted C. Decimal literals
can represent the full range, including `(u64 18446744073709551615)`.
`u64.add.wrapping` adds modulo 2^64. `u64.eq` and `u64.lt`
compare two exact `u64` values and return `bool`.

Native `i64` values use `int64_t`. Decimal and hexadecimal literals
cover the full signed range, including `-9223372036854775808i64` and
`(i64 -0x8000000000000000)`. `i64.eq` and `i64.lt` compare exact signed
operands. `i64.add.wrapping`, `i64.sub.wrapping`, and
`i64.mul.wrapping` keep the low 64 bits. The
[wide decimal writers](text/decimal_wide.wove) append either word type
to a bounded byte view and preserve any prefix written before exhaustion.

Both unsigned word widths support `and`, `or`, `xor`, and `not`
with a type prefix such as `u64.xor`. The first three forms take two
words of the named type. Complement takes one. Shifts such as
`(u64.shr word count)` take a word of the named type and an exact
`i32` count. The left shift discards bits above the word width and
the logical right shift fills with zeroes. Word operations return the
named word type and evaluate each operand once, from left to right.
Counts outside `[0, 32)` for `u32` or `[0, 64)` for `u64` trap at
the original file ID and half-open span of the shift.

`(int.convert target value)` evaluates the integer operand once and
preserves its numerical value when it fits the target `i8`, `u8`, `i16`,
`u16`, `i32`, `u32`, `i64`, or `u64`. A value outside the target range traps before the cast. The
runtime diagnostic identifies the original file ID and half-open byte
span of the conversion, including from a source subrange or an imported
file. C emitted for conversions includes `wove_runtime.h` and links
with the shared Wove C runtime. `(u8.from.u32 value)` applies the same
check after requiring an exact `u32` operand. The `<destination>.from.<source>`
form supports every pair of builtin integer types.

`(T.fits value)` checks representability as a `bool` before a caller
chooses whether to perform a conversion. For arithmetic with a recoverable
result, write `(T.try_mul left right (borrow-mut place))`. The same form
supports `try_add`, `try_sub`, `try_div`, and `try_rem`. Both input
values and the destination have exactly type `T`. Input evaluation
finishes in left-to-right order before the output loan starts, so
`(i32.try_add (local count) 1i32 (borrow-mut (local count)))` can update
the value it read. The writeback occurs only when the expression returns
`true`. For any signed width, minimum divided by `-1` returns `false`,
and minimum remainder `-1` writes zero and returns `true`.

For any builtin integer `T`, `(T.to.le-bytes value)` and
`(T.to.be-bytes value)` evaluate an exact `T` once and return an inline
`(array u8 N)`. The width `N` is one for `i8` and `u8`, two for
`i16` and `u16`, four for `i32` and `u32`, and eight for `i64` and `u64`. The inverse `T.from.le-bytes`
and `T.from.be-bytes` forms take a shared byte view. They trap at the
conversion span before reading if the view has fewer or more than `N`
elements. Signed values use two's complement. The byte order is explicit
on every host, and the view may start at any byte offset. For example,
`(u32.from.le-bytes (borrow-shared (array.range (local source) 1i32 5i32)))`
reads four bytes starting at index one.

Stage 2 uses native C `float` and `double` for `f32` and `f64` in locals,
function signatures, struct fields, and C bridges. Write a typed decimal
as `(f32 -2.5)` or `-2.5f32`. A decimal needs digits before the point and
in each written fraction or exponent. Clang rounds the original digits at
the written width and preserves a negative sign on zero.

Use width-qualified `add`, `sub`, `mul`, `div`, and unary `neg` for native
floating arithmetic. The `eq`, `ne`, `lt`, `le`, `gt`, and `ge` operators
take two values of the named width and return `bool`. Eager operands evaluate
once from left to right.

Use `(f32.from.bits word)` and `(f32.to.bits value)` to copy the representation
between `f32` and `u32`. The matching `f64` forms use `u64`. These operations
retain signed zero and NaN payloads. For example, an STL coordinate read from
little-endian bytes can widen with `(f64.from.f32 (f32.from.bits (local word)))`.

The same `<destination>.from.<source>` syntax handles checked numeric conversions
involving floats. Floating-to-integer conversions truncate toward zero and
trap at the conversion span if the result cannot fit, including for NaN and
infinity. `f32.from.f64` keeps native rounding, subnormals, and nonfinite
values, and traps on finite values outside the binary32 range.

A `for` statement has a binder, a source, and a body. A half-open range uses
`(for (bind cursor) (range start end) (body ...))`. Both bounds must have the
same type, `i32` or `u64`. The loop evaluates them once, left to right, and
skips equal or reversed bounds.

To read an array, use `(for (bind item) (borrow-shared (local data)) (body ...))`.
The shared loan lasts for the loop, and the body gets a copy of each element
in source order. Use `(bind index item)` for an additional `i32` position.
The index starts at zero in the captured view, including a checked byte
subrange such as `(borrow-shared (array.range (local data) start end))`.
A module byte string can supply either the whole view or a subrange.
Range bindings, indexes, and shared items are immutable and scoped to the
body. `continue` advances to the next iteration. Like `break` and `return`,
it releases owners in the scopes it exits.

For writable iteration, use `(for (bind item) (borrow-mut (local data))
(body (set (local item) value)))`. The loop holds an exclusive view. Each
item names its original element and can be written or reborrowed for a call.
An optional index stays immutable and relative to the view. To fill a byte
window, borrow a checked `array.range` in the header or reborrow a named
mutable view. Access through the source resumes after the loop exits.

A `let` can bind a shared or mutable view of a named local array or array
parameter. The binding itself is immutable. A mutable view permits writes
through its descriptor. For example, `(let window (borrow-mut (array.range
(local output) start end)))` holds a checked byte range and can lend it to
several helpers through `(borrow-mut (local window))`.

The loan lasts to the end of the binding's block. A shared view permits reads
of the source and excludes writes, replacement, and moves. A mutable view
also excludes reads through the source. Reborrows follow the same rule for
their immediate parent view. Each scope exit, including `break`, `continue`,
and `return`, ends its view loans. A view uses the source's existing storage.
To borrow a range from a fixed inline array, first bind a view of the whole array.

Helpers can take `(shared-array u8)` parameters using the runtime's
`wvrt_slice` ABI. Pass a named view to another helper with
`(borrow-shared (local data))`. `(array.len (local data))` returns its
signed `i32` length. `(array.load (local data) index)` takes an exact
`i32` index and returns a `u8`. A negative index or an index at or beyond
the length traps before reading the slice, with the original file ID and
half-open byte span of the load. C emitted for a shared view includes
`wove_runtime.h`. Link the shared runtime when the code uses a checked load.

A `(shared-array Token)` parameter can refer to a named, copyable scalar
record. Its C descriptor is also a `wvrt_slice`. Pass a whole view to
another function with `(borrow-shared (local data))` and the same defining
element type. `array.len` counts records and `array.load` checks its
`i32` index before copying the selected record by value. A C caller
supplies the original array of the emitted record type and its element
count and size.

Helpers can also take `(mut-array u8)` parameters using `wvrt_mut_slice`.
Pass one to a writable parameter with `(borrow-mut (local data))`, or
to a shared parameter with `(borrow-shared (local data))`. An explicit
reborrow lasts through the call. A later argument cannot use the same
parameter while it is exclusively borrowed, and a shared loan prevents
an exclusive one until the call returns. Both view types support
`array.len` and `array.load`.

`(array.store (local data) index value)` is a statement. For a byte
receiver, it requires a mutable owner or exclusive view, an exact `i32`
index, and an exact `u8` value. It
evaluates its operands in order and checks bounds before writing the
byte in the caller's storage. An out-of-bounds store reports the same
file identity and half-open span format as a checked load.

A `(mut-array Token)` carries the same defining record type as its
shared counterpart and uses the `wvrt_mut_slice` C descriptor.
Use `(borrow-mut (local data))` to pass writable access to a helper.
A shared reborrow keeps the same record element and gives read access.
A store checks its `i32` index before replacing one element with a value
of the exact defining record type.

A function-local `let` or `let-mut` can own `(array.alloc u8 length)`.
The length is an exact `i32`. The runtime allocates zeroed bytes and
reports an invalid length or allocation failure at the allocation's
original file identity and byte span. A shared call borrow keeps the
owner's original storage. A mutable call borrow requires `let-mut`.

Use `(array.store (local output) index value)` to fill a local byte array
bound with `let-mut` before returning it by move. The store holds an
exclusive view of the live owner while evaluating the index and value,
then checks the index and writes the byte. A call in either operand
can borrow a different buffer. The index and value have the same exact
`i32` and `u8` types and bounds diagnostics as a store through a slice
parameter.

For numeric elements, `(array.alloc i32 count)` and `(array.alloc f64 count)`
create zero-initialized owners with a runtime `i32` length. Shared and
mutable parameters such as `(shared-array i32)` and `(mut-array f64)`
borrow their original storage. Loads and stores check an `i32` index, and
the value has the exact element type. A zero-length owner can be moved
and borrowed, and every indexed access to it traps.

For a copyable record, `(array.alloc Token count)` owns `count` zeroed
records. The runtime allocates with `sizeof` and `_Alignof` of the
defining C type. A whole-array call borrow uses a slice of the same
defining record and the allocation's original storage. A `let-mut`
binding permits exclusive access. A helper with `(returns (owned-array Token))`
can return a fresh allocation or move a local owner of that exact element type.
The caller owns the returned allocation and releases it on scope exit.

`array.len` also reads the length of a live owner or the extent of a fixed
array. It evaluates a computed fixed array receiver before returning its
extent.

Wove functions can require an exact element count with
`(shared-array u8 FACET)` or `(mut-array u8 2)`. The optional extent uses the
same decimal or immutable module integer syntax as an inline array. A
whole inline array, a reborrow of an exact parameter, or a numeric range with
nonnegative literal endpoints can prove the argument's length. The usual
range bounds checks still apply. If the length is unknown at compile time,
the compiler checks the evaluated descriptor at the call argument before
any later arguments run. An exact view can reborrow into a parameter that
accepts any length.

A numeric range argument uses `(borrow-shared (array.range (local source) start end))`
or the same projection with `borrow-mut`. An exclusive range requires a
writable owner or mutable view. The source view and its exact `i32` endpoints
are evaluated once, left to right. The runtime checks
`0 <= start <= end <= source.len` before entering the callee. The view
uses the original storage and exact element type, with length `end - start`
and index zero at `start`. The loan lasts from projection through the call,
including later argument evaluations. A bounds failure reports the range's span.

Owners are released in reverse declaration order when their lexical
scope exits, including on each completed loop iteration. A return
evaluates its result first and releases the active owners from the
scopes it leaves.

`(let received (move (local data)))` transfers the existing allocation
between local bindings in the same lexical block. Completed call borrows
allow the move, and the receiving binding controls mutable access. Cleanup
drops the receiver once. An earlier exit before the transfer drops the
original owner. Borrowing or moving the old binding again reports
`ownership.use_after_move` with the attempted operation and the original
move span.

A Wove helper can declare a numeric owner result such as
`(returns (owned-array i32))` and transfer an array with `(return (move (local data)))`. This transfers the original
allocation on that exit while releasing the other live owners. An
alternate path still sees the local until it transfers or releases it.
A wrapper can also return the fresh owner from a call directly. The
caller binds the result with `let` or `let-mut` and can read, borrow, or
move it through its normal owner lifetime. A zero-length result follows
the same ownership rules.

`(array.alloc (owned-array u8) count)` allocates a vector with initially
unfilled row slots. Use `(array.store (local rows) index (move (local row)))`
or a call returning a fresh byte owner to fill or replace a slot. A
replacement releases the previous row. A loan such as
`(borrow-shared (array.load (local rows) index))` borrows the stored byte
slice for the call and reports an unfilled slot at the indexed access.
The vector owns its filled rows and can be returned through
`(returns (owned-array (owned-array u8)))`.

`(bool.and left right)` evaluates the right operand when the left is true.
`(bool.or left right)` evaluates it when the left is false. Both operands
are type-checked as `bool`. Each operand that runs does so once, with the
left first. `(bool.not value)` negates its single `bool` operand. All three
forms return `bool`.

A statement `(if condition (then ...) (else ...))` checks its condition
as `bool` and executes only the selected arm. An omitted `else` falls
through when the condition is false. Each arm starts its own scope and
can read or assign visible outer bindings. A local can shadow an outer
name after its initializer and stays in scope only within its arm.
A return exits the function from either arm. When both arms return,
the conditional is terminal.

A statement `(while condition (body ...))` evaluates its `bool` condition
before each iteration, including the first. Each iteration has a fresh
body scope. Updates to an outer mutable binding are visible to the next
test. A return inside the loop exits the function.

`(break)` leaves the innermost loop and `(continue)` restarts it at its
condition check. Both forms take no operands and require an enclosing loop.
Each exit releases the active owners introduced in the iteration or its nested
scopes. A continue preserves owners that were live at loop entry.

A loop with a nonliteral predicate can skip its body. A literal
`(while (bool true) (body ...))` reaches its successor only through a reachable
break targeting that loop. A break in an inner loop or an arm excluded by a
literal condition cannot make the outer loop continue.

[FNV-1a](hashing/fnv1a.wove) exposes the immutable 31-bit seed `fnv1a_seed`
and updates it one raw byte at a time with `fnv1a_byte`. The declaration
identity builder supplies the order and delimiters for its components.

[Base32hex](hashing/base32hex.wove) exposes the fixed width `base32hex31_width` and
`encode_base32hex31`. The encoder writes all seven digits at the requested
offset and returns true. An out-of-range ID or output span returns false
and preserves the destination.

The [C name generator](emit/text.wove) takes a `CNameRole`, a checked ID,
the source name's byte span, and a translation unit prefix. The seed represents
the role enum with a record and named constructors. The default spelling is
`wg<role><id><suffix>`, where `wg` stands for Wove-generated. The character
after the prefix identifies the role. The ID uses seven lowercase
Base32hex digits and starts with `0` or `1` because IDs are 31 bits.
A suffix keeps ASCII letters, digits, and underscores. Other bytes
become underscores. Graph emission reserves a prefix against selected
native function names, type spellings, and destructors before writing C. It uses the first available prefix from
`wg`, `wwg`, `wwwg`, and so on, keeping the same roles, IDs, and suffixes.
Native symbols and the ABI entry `main` retain their original spellings.

| Tag | Role | ID | Suffix |
| --- | --- | --- | --- |
| `f` | Function | Stable declaration hash | Function name |
| `r` | Record | Stable declaration hash | Record name |
| `m` | Record member | Zero-based position in its layout | Empty |
| `p` | Parameter | Zero-based position in its signature | Parameter name |
| `l` | Local | Declared name's original file-local tree row | Local name |
| `t` | Expression temporary | Original expression's file-local tree row | Empty |
| `j` | Loop break label | Loop statement's original file-local tree row | Empty |
| `q` | Read-only C type alias | Binding name's tree row in the translation unit | Native name |
| `b` | C-string call storage | Original call's file-local tree row | `a` for descriptors, `b` for backing storage |

For a single module, hash its declared name, the four-byte little-endian
declaration kind, and the original declaration name using the archive's
31-bit FNV-1a. Records use the archive's struct kind 1, and functions use
kind 2. The ID is independent of source layout and
declaration order. Detect collisions before writing C, even when the
readable suffixes differ. Each resolved call keeps the callee's defining
row and its own source node for later analysis.

The result gives the written count and a view-relative span. Successful
emission spans the module. A failure preserves the destination bytes and
uses the originating span. Tree and module diagnostics keep their codes.

| Code | Failure | Primary span |
| --- | --- | --- |
| 16 | Short C output view | The selected entry |
| 22 | Missing entry or unsupported declaration | The module, first function name, or offending declaration |
| 23 | Unsupported signature or record field type, or invalid native bridge contract | The offending signature or contract form or operand |
| 24 | Invalid, unsupported, or unreachable body form, or incompatible expression type | The offending form or expression |
| 25 | Invalid scalar literal | The value or malformed literal form |
| 32 | Stable declaration identity collision | The later declaration name |
| 33 | Duplicate function, parameter, local, record, or field | The later name |
| 34 | Unavailable function, binding, record, or field | The requested name or incomplete reference |
| 35 | Wrong call arity | The call expression |
| 37 | Assignment to an immutable binding | The target name |
| 39 | `ownership.use_after_move` | The attempted use, with the original move as a related span |

The [caller inspector](source/callers.wove) uses the checked scalar plan
from before C emission. Each call records its caller and callee as a
defining file identity and declaration tree row. Its half-open byte span
is relative to the original file, including when the module is inspected
through a subrange. Records follow source order, with one per call site.
An unchecked module reports its original diagnostic. Code 36 means the
requested function is unavailable in the selected module. Code 16 reports
the first incoming call that exceeds the supplied output capacity.

The [graph resolver](source/graph.wove) reads a supplied virtual file table
and binds active functions, records, and immutable values in separate
namespaces. The table maps normalized absolute paths to defining file
identities and source ranges in a shared byte arena. Relative imports use the directory of the reached path.
Stage 2 resolves virtual paths lexically before lookup, so a directory
discarded by `..` can be absent from the graph. Stage 1 requires each
intermediate directory to exist on the host filesystem.
Several paths can refer to one identity, which the resolver reads once.
An empty `only` still visits its provider. An excluded module contributes
no names or nested imports. The resolver applies one mode across the graph,
including C header providers and their selected members. The resolved graph
retains its mode for signature checking and C emission.

In a `wove-file` selection, a bare name or `(as name alias)` selects
a function or an immutable module binding. When both share a name in
the provider, the unqualified selector picks the function.
`(value name)` or `(value (as name alias))` selects the immutable
binding explicitly. Both aliases and direct selections use
`(local name)` and read the same defining storage.

`(type Token)` or `(type (as Token LocalToken))` selects a record.
Function, type, and value selections use separate namespaces. Each
selector binds a declaration defined by the provider.

A `use c-header` selects functions, native types, and values from a
`bridge c-header` with the same decoded header key in the supplied source table. A key such as `"<stdlib.h>"`
or `"./project.h"` identifies one active bridge in a well-formed canonical file.
Header lookup leaves Wove-file reachability unchanged. A selected declaration
has `params` and `returns` forms. Both support exact `bool`, `u8`, `i32`, and
`u32` types. Shared and mutable byte-slice parameters require an explicit
native argument contract. Parameter names must be unique within the signature. The alias occupies the
importing file's function namespace. Its binding keeps the defining declaration
and the raw header, native symbol, and signature spans in that file.

A bodyless C bridge function requires one `params` and one `returns` and
can specify one quoted native target with `c-name`. It accepts these fields
and metadata in any order after the Wove name.

```lisp
(fn absolute (returns i32) (:doc "Call C abs.")
  (params (value i32)) (c-name "abs"))
```

The Wove selector uses the declared function name. Calls and C ABI checks use
the target's C identifier. Without `c-name`, the two spellings are the same.

Attach `(:pragma (c_optional))` to a bodyless bridge function to check its
availability when native `cc` or `bin` resolves a selection. Clang must find
a `FunctionDecl` whose name exactly matches the C target. Other bridge members
stay usable regardless of an unselected optional declaration. The compiler
reports a selected missing function at its original Wove selector, including
when the selector gives it a local alias.

The probe uses resolved C flags, definitions, and the selected C dialect.
For a quoted project header, retained-only `cc` also searches the directory
of the Wove bridge. A binary build probes from its private build directory
with the same frontend arguments and staged runtime headers used for
compilation. The probe uses preceding C includes in emission order and
checks for declarations introduced by the selected header. Dependency and compilation database output options apply to the final
build. Availability errors use the importing source span.

| Code | Meaning |
| --- | --- |
| 71 | Optional C function unavailable |
| 72 | C header probe tool failed |
| 73 | C header probe output malformed |
| 74 | C header probe workspace exhausted |
| 75 | C header probe setup failed |

The native `query_clang_header` API in `clang/query.wove` accepts a Clang
executable, NUL-terminated target and include arguments, the header spelling,
and the originating import span. It renders the embedded C probe, captures
one AST document, and returns an index for exact `clang_declaration` lookup
with the caller's declaration rows and decoded keys. For quoted project
headers, pass the importing directory in the include options.

A successful probe distinguishes present and absent headers through the
probe's enum constant. Tool failures, truncated stdout, malformed probe
output, and insufficient AST workspace have separate result codes. The
query carries the import span for diagnostics and keeps the child status
and JSON error details available to the caller.

Bridge `const`, `let`, and `let-mut` declarations provide typed values
from the C header. A bare selection tries functions first. Use
`(value O_CLOEXEC)` or `(value (as errno error_indicator))` to select
from the value namespace explicitly. Constants use the original C
expression, and the generated C checks that it is an integer constant
representable in its declared type. `let` provides live read-only
access. `let-mut` also permits `(set (local error_indicator) (i32 0))`
through the original C lvalue, including macro-backed `errno`.
The right-hand value is evaluated before resolving the lvalue for the write.

A bridge `(let stderr (shared-ref File))` exposes a library-owned
pointer for a shared C parameter. Pass it directly as `(local stderr)`
or with `(borrow-shared (local stderr))`. The call checks that the
handle is present and keeps ownership with libc.

The scalar graph compiler checks each C call against the selected Wove
signature and evaluates its arguments once from left to right. The emitted
call uses the original native name. Each selected header contributes one
include operand. Angle-bracket keys use system search. Bare or explicitly
quoted project paths use quoted includes. The C compiler reads the native
declarations from those headers.

Mark a value-returning function-like macro with `(:pragma (c_macro))`
after the bridge function name. Macros use the same exact signatures
and native argument contracts as C functions, including borrowed
references and converted C strings. The emitter captures each argument
in a typed temporary before expanding the original macro spelling.
Argument expressions run once in source order even when the macro
repeats or discards an operand. The invocation's result is stored in
its declared Wove type.

A read-only C bridge value can use `(let (:pragma (c_macro)) HUGE_VAL f64)`
to bind a scalar expression macro. The emitted C checks the expansion's
exact type without evaluating it. Each Wove read expands the original
native name, including through an imported alias.

Use `(:pragma (c_cstr parameter ...))` or `(:pragma (c_cstr_raw parameter ...))`
after the bridge function name. Shared byte slices pass `const char *`.
Mutable byte slices pass `char *`. After evaluating all arguments, the call
copies each named slice and appends a NUL byte. Checked mode traps at the
Wove argument span on an embedded NUL before calling C. Raw mode copies
every byte, including bytes after an embedded NUL. An empty view passes a
pointer to one NUL byte. After the call and any result snapshot, Wove copies
back the original length of each mutable view and releases the temporary
storage. Copy-back also runs when C returns null.

Use `(:pragma (c_variadic first_tail_parameter))` to bind one typed use
of a C variadic function. The named parameter follows at least one fixed
parameter. Every Wove argument is required, including the named tail and
all parameters that follow it. The tail keeps its declared Wove types
through checking and capture. The emitter verifies the fixed prefix
and result against the header's variadic prototype. Fixed C byte views
accept compatible void or character pointers with their access mode.
Clang applies the target C ABI's default argument promotions at the call.
This includes `open` and `openat` with the native `Mode` alias. Repeated
boundaries must agree. A named boundary and `c_macro` cannot share a
declaration.

A native fixed prefix can supply a header constant with
`(const command i32 F_SETFL)` in its parameter list. A call to
`fcntl_setfl` supplies the descriptor and flags. Stage 2 inserts
`F_SETFL` between them for the original `fcntl` symbol, includes the
fixed command type in the prototype check, and checks that the header
constant fits `i32`. Fixed operands may also appear in nonvariadic
native functions.

Use `(:pragma (c_bytes data count))` for a byte count or
`(:pragma (c_bytes data size count))` for a product. The named dimensions
accept `u8`, `i32`, or `u32` formals. After argument evaluation, both
dimensions must be nonnegative and their extent must fit in the selected
view length. Stage 2 compares by division to avoid overflow. Zero extents
can lend empty views. The check traps at the data argument before entering
C. A shared view passes a read-only pointer, and a mutable view passes a
writable pointer to the same storage for the duration of the call.

Use `(:pragma (c_array data count))` when a C argument points to a
bounded array of integers. The extent is a nonnegative decimal `i32`
literal or the name of a C formal declared with `u8`, `i32`, `u32`, or
a native integer alias. For a 64-bit count, declare an alias such as
`(type Count (c-type "uint64_t"))` and use `Count` as the formal's type.
The check uses the number of elements in the slice and traps at the
data argument before entering C if the count is negative or too large.
A shared slice passes a `const` pointer to the exact declared element
type. A mutable slice passes a writable pointer, so writes by C reach
the caller's original storage. A zero count can lend an empty view.
Built-in elements can use a fixed integer array. Native integer aliases
can use an owner from `(array.alloc ElementType count)`. For example,
`<unistd.h>` declares `pipe` with `(mut-array i32)` and
`(:pragma (c_array descriptors 2))`.

A bridge can declare an integer alias such as `(type Size (c-type "size_t"))`.
Select it with `(type Size)` or rename it in the importing file. A function
signature resolves each alias in its defining bridge or Wove namespace.
Values carry that defining type through parameters, results, and locals.
The emitted C keeps the header's native spelling and checks its integer
representation. Distinct Wove declarations have distinct types even when
their C spelling is the same. Checked conversions and native alias
dimensions in byte extents are tracked in [#198](https://github.com/wove-lang/wove/issues/198).

A C bridge begins with its provider and quoted header. Its contiguous
metadata prefix follows the header. Bridge-level cfg applies to the whole
bridge, and each member may carry its own metadata.

A project bridge for a bundled system header adds members to the header's
namespace. A project member replaces the bundled member with the same Wove
name in its namespace. Both sets of signatures can resolve types in the
combined header. Put `(:pragma (bridge_replace))` after the header to use
only the project declarations. Target cfg selects the active bridges before
duplicate project keys are diagnosed.

A pointer-backed resource declaration pairs a native C type literal with its
destructor contract.

```lisp
(bridge c-header "<stdio.h>"
	(:doc "Declare owned stdio handles.")
	(type (:pragma (c_owned (drop fclose))) File (c-type "FILE *"))
	(fn tmpfile (params) (returns File))
	(fn fgetc (params (stream (shared-ref File))) (returns i32)))
(use c-header "<stdio.h>" (only (type File) tmpfile fgetc))
```

A resource result resolves to its type in the defining bridge. Type selectors
can alias the name in the importing file. The decoded `c-type` uses C type
words followed by one or more pointer stars. The header supplies the native
declarations. Stage 2 keeps the pointer directly in a local owner and
preserves NULL as a missing handle. Scope cleanup calls the declared
destructor once for each present handle. Returns evaluate their value before
cleanup. A local `move` transfers cleanup to the receiving binding.

A bridge parameter spelled `(shared-ref File)` borrows that exact resource
type from a local owner. For example, `(call fgetc (borrow-shared (local file)))`
checks the handle at the borrow's source span and passes its original C
pointer. A missing handle traps before the call. The loan ends on return
from C, and the local owner retains its destructor responsibility.

Use `(owner.is-null (local file))` to test a named native owner without
borrowing it. A bare owner type in a C signature consumes that resource.
For example, `(call fclose (move (local file)))` requires the exact `File`
owner and traps at the move on a missing handle. The transfer clears the
local slot, and C becomes responsible for the handle even when the call
returns an error. Lexical cleanup drops any handle still present on the
executed path. Later uses on any continuing path that could have
transferred the owner point to the use and its earlier move.

Use `(:pragma (c_snapshot cstr))` for a borrowed C string result. Wove copies
the bytes up to the first NUL while any argument backing is still live.
Use `(:pragma (c_snapshot cstr (drop free)))` for an allocating result and
release each present C result exactly once after copying. The release has C
signature `void (void *)` or `void (char *)`. Both forms require
`(returns (optional (owned-array u8)))`. C null stays absent and an empty C
string produces a present zero-length owner.

For a returned native record pointer, `(:pragma (c_snapshot cstr (field name)))`
copies an inline `char` array into the same optional owner. The C compiler
checks the selected field's type and complete bound. Wove evaluates the native
call once, preserves null and `errno`, and traps at the call site if the
declared field extent contains no NUL. The bundled `readdir` takes an
exclusive directory-handle borrow and snapshots the native API's terminated
`d_name`, including names from extended records.

Use `(:pragma (c_cstrs arguments environment))` on shared views of owned
byte rows. Each argument supplies a detached `char *const *` table with a
null terminator and a separate terminated string per row. An empty vector
still provides its terminator, and an empty row provides one NUL byte.
Embedded NUL bytes trap at the vector argument. Native edits to the copies
leave the original Wove rows intact.

A read-only native `let` with `(:pragma (c_snapshot cstrs))` reads a
`char **` global such as `environ`. Each local initializer copies the
table and its rows into `(owned-array (owned-array u8))`. A null table
yields an empty owner. The C table and rows must stay valid until the
copy completes.

A fixed C signature can declare `(mut-ref T)` for writable scalar storage.
Use `(borrow-mut (local value))` on a mutable local or borrow a writable
record field with its exact declared type. C receives a pointer to that
storage, so changes are visible when the call returns. Integer aliases
keep their defining identity and original C pointer type. The loan remains
exclusive while evaluating later arguments and running the call.
A fixed signature can mark a mutable scalar reference with
`(:pragma (c_nullable parameter))` to also accept the bare `null` literal.
For example, the checked-in `waitpid` accepts either a mutable `i32` loan
or `null` for its status parameter.

For a pointer-backed owner, `(:pragma (c_init output))` on a formal such
as `(output (mut-ref Database))` passes the address of the native handle
slot to C. For example, a `sqlite3 *` owner gives C a `sqlite3 **`.
Declare a mutable local with `(native.empty Database)` and pass
`(borrow-mut (local db))` to the initializer. The loan requires an empty
slot and traps at the borrow span if a handle is already present. C may
write the slot during the call and must release its access on return.
The native header's prototype must match the declaration.

The call returns the native `i32` status. Test the owner separately with
`owner.is-null`. A null output leaves the slot available to retry. Any
nonnull output belongs to the local owner even on failure, as required by
`sqlite3_open`. An explicit move transfers responsibility and scope exit
destroys any handle still present.

The `storage` flag in `(:pragma (c_owned (drop destroy) storage))`
declares a caller-owned C object. Initialize a local with
`(native.alloc Actions)` to obtain zeroed storage using the definition's C
size and alignment. A mutable loan to a formal named by
`(:pragma (c_init actions))` requires inactive storage. The C call
receives the object's address and returning `0i32` makes it active.
A failed initialization leaves the storage inactive for retry or cleanup.
Ordinary borrows and explicit consumption require an active object.
Repeated initialization of an active object traps at the argument span.

A shared loan passes `const T *` and a mutable loan passes `T *` for the
declared object type `T`. Scope exit calls the declared destructor only
for active storage and always releases the allocation. An explicit
consuming call takes `(move (local actions))`. It receives the same
object address and takes responsibility for destruction. Wove releases
the allocation after the call returns. C may use the borrowed or
consumed address for the duration of that call.

A fixed C signature can use `(c_null resolved)` for a byte-view formal.
That slot requires bare `null` and passes C `NULL`, including when the
slot also has a `c_cstr` conversion. For example, `(call realpath
(borrow-shared (local path)) null)` uses the allocating libc path. Test
the result with `(owner.is-null (local result))` and extract its byte
owner with `(move (optional.unwrap (local result)))`. The unwrap traps
at its source span if the result is absent.

Each binding identifies the importing namespace and the name span in that
file. Its target is the defining file identity and declaration node index
in the defining source tree. Earlier reader diagnostics keep their codes
and add the identity of the file where they originated.

| Code | Failure | Primary span |
| --- | --- | --- |
| 16 | Short graph storage | The next row's source node, when available |
| 26 | Malformed use, selector, declaration name, or C header operand | The originating form or operand |
| 27 | Missing active provider | The importing use |
| 28 | Active import cycle | The closing use |
| 29 | Unavailable function or type | The requested name |
| 30 | Conflicting binding or C bridge declaration | The new local name or conflicting declaration |
| 31 | Excluded entry module | The entry module |

The [scalar graph compiler](emit/graph.wove) consumes the resolver's
active modules and populated bindings. It checks all reached scalar
signatures and resolves every direct call in the caller's file before
emitting C. Signatures and constructors resolve record names through
the current file's type bindings. Each record keeps one C typedef
across aliases, and each function has one symbol across aliases and
diamond edges. Its graph symbol uses the defining file's canonical virtual
path and declared module name for the namespace. Library functions
named `main` have ordinary scalar signatures. The root's `main` keeps
the executable ABI.

The graph caller inspector uses the same checked plan. Query a name
through a file's local bindings to inspect its defining declaration.
Every call site keeps the original file and declaration identities
of both endpoints. Diagnostic spans use the file where the failure
originated.

`make` builds `build/wovec` from [the compiler root](wovec.wove). The Python
seed emits the bootstrap entry for `build/stage-1/wovec`. That compiler
opens the checked-in entry and follows its active imports and embeds to
build `build/stage-2/wovec`. The second compiler runs its native `bin`
command on the same entry to build the final compiler. Each generation
compiles and links one C translation unit with Clang.

The native generations read the same copy of the source tree and runtime
under `BUILD_DIR/self-host-source`. Its local `build/version.txt` supplies
the exact same bytes to both compilers, including with a custom
`BUILD_DIR` or version override. `make self-host` compares their retained
C with C re-emitted by the final compiler and builds and runs a smoke
program using that compiler.

The seed translates a separate [Wove fixture root](../tests/stage-2/source_scan.wove)
for the [C source probe](../tests/stage-2/source_scan.probe.c). Run
`make test-stage-2` to exercise the bootstrap entry and compare scanner
streams with the Stage 1 lexer. The
[layout fixtures](../tests/stage-2/lexer_layouts.tsv) supply indentation, line
endings, and parent view bytes. The parity check compares token kinds and raw
spellings. `make test` includes these checks.

The seed-built fixture executable accepts `--tokens FILE [START END]` to
print numeric rows with kind, start, end, error, line, and column. The span
is relative to the selected view and the coordinates use the whole file.

Use `--form FILE [START END]` to check structure. The single result row
contains the row count, start, end, error, line, and column. The same
view-relative spans and file coordinates apply.

Use `--tree FILE [START END [TREE_CAPACITY]]` to build a tree. It emits the
form check result followed by the populated nodes. When supplied, the
capacity is an element count. The fixture verifies that spare storage
retains its original bytes.

Use `--metadata FILE [START END [TREE_CAPACITY]]` to validate the built
tree. On success it emits the same rows as `--tree`.

Use `--render FILE [START END [OUTPUT_CAPACITY]]` to render the validated
tree. The first output line contains the written byte count and error
code, separated by a tab. The remaining output is the exact written prefix.
The probe checks guard bytes around the output view and compares unwritten
bytes against their original values. The
[rendering fixtures](../tests/stage-2/source_renderings.tsv) compare the
original and reparsed trees and tokens on each successful render.

Use `--decode FILE [START END [OUTPUT_CAPACITY]]` with a view whose first
token is a string. The first output row contains the written count, span start,
span end, error, line, and column. The rest is the decoded prefix as raw bytes.
The [decode fixtures](../tests/stage-2/source_decodings.tsv) check spans,
compare successful values with Stage 1, and check the prefix on failure.

Prefix a selection, compilation, or caller inspection command with
`--mode bootstrap`, `--mode native`, or `--mode test` to choose its mode. The
probe defaults to bootstrap mode. Native mode accepts the two-parameter argv
entry. Test mode supports declaration selection and inventory discovery.

Use `--module FILE [START END [DECL_CAPACITY]]` to select a checked module.
The first row contains the selected count, start, end, error, line, and
column. The second row gives the name node index and the active flag.
When the module has a checked name, the remaining rows describe the module
and the selected declarations. Each lists the original form, head, first
metadata, first ordinary field, and name indices. The
[module fixtures](../tests/stage-2/source_modules.tsv) compare these links
and the active declarations with the Stage 1 seed.

Use `--emit-c FILE [START END [OUTPUT_CAPACITY]]` to compile a local scalar module.
The first row gives the written count, source span, error, line, and column.
On success the remaining bytes are the C translation unit. An ownership
error with related context adds a `related` row with the file identity,
span start and end, and one-based line and column. Local emission and
caller inspection use view-relative spans and parent-file coordinates.
Graph emission and inspection use spans relative to the defining file. The
[compilation fixtures](../tests/stage-2/source_compilations.tsv) compile the
emitted C with Clang, run the executable returning 42, and check the full
return values at the i32 boundaries. They also cover stable helper names,
suffix and identity collisions, and source locations for invalid calls.

The [graph fixtures](../tests/stage-2/source_graphs.tsv) pass virtual paths
and raw source files to `--graph ENTRY CAPACITY (IDENTITY PATH FILE)...`.
The probe prints the reached module rows and local bindings with their
defining identities and node indices. It prints `binding` for functions
and `type-binding` for records. For a C function, `c-binding` gives the
same binding fields. The following `c-target` row gives the raw header,
native symbol, and signature span pairs in the defining file.
A failure gives the code and byte span with one-based coordinates in its
original file.

Use `--callers FILE NAME [START END [CAPACITY]]` to inspect a function by
its Wove name in a library module. Use `--callers-root` with the same
arguments for an executable root, where `main` follows the C ABI rules.
The single-file probe assigns file identity 0 to the input.
The header gives the caller count, view-relative span, error, line, and
column. A successful result contains a `declaration` form with computed
`:callers` metadata and the declaration's identity. Each `call-site` gives
caller and callee file and node identities, their source names, and the
original file-relative span. A function with no incoming calls has
`(:callers)`. The [caller fixtures](../tests/stage-2/source_callers.tsv)
check repeated sites, an empty result, bootstrap selection, and file offsets
after layout and module-name changes.

Use `--emit-graph ENTRY SCRATCH CAPACITY (IDENTITY PATH FILE)...` to emit
one C translation unit from a supplied scalar graph. Use
`--graph-callers ENTRY SCRATCH FILE NAME CAPACITY (IDENTITY PATH FILE)...`
to inspect the definition selected by `NAME` in the supplied `FILE` row.
`ENTRY` and the query's `FILE` are virtual path row indices.
`SCRATCH` bounds the import-resolution tables. Output capacity counts
bytes for emission and call sites for inspection. The success header
gives the output count, defining file identity, and file-relative span.
The [scalar graph fixtures](../tests/stage-2/scalar_graphs.tsv) check the
native result, defining symbols, repeated imports, and original call spans.

## Expectation inspection

Use `--graph-expect ENTRY SCRATCH FILE NAME CAPACITY (IDENTITY PATH FILE)...`
with the supplied-graph probe to read `:expect` links attached to a
selected declaration by its declared name. `FILE` selects a virtual path
row and may identify an alias of the same source file. Use
`--graph-expect-link ENTRY SCRATCH issue LINK CAPACITY (IDENTITY PATH FILE)...`
or replace `issue` with `test` to find the owners of an exact link value.
A compact `gh:` reference and its full URL are distinct link spellings.

Each link row gives the canonical file identity, file-local declaration
row, and half-open byte spans for the name, attached `:expect` form, and
link token. The row also reports the kind, name, and original token text.
The reverse query visits active files in supplied table order and
declarations in source order. A repeated link retains every occurrence.
Capacity counts link rows. Metadata inside a body does not attach to its
enclosing declaration. The [expectation fixtures](../tests/stage-2/expectations/cases.tsv)
cover both query directions and original spans across files.

## Inline test discovery

Use `--mode test --graph-tests ENTRY SCRATCH CAPACITY (IDENTITY PATH FILE)...`
with the supplied-graph probe to discover tests. Mark a function with
`(:pragma (test))` and give it `(params)` and `(returns void)`. The same
mode selects test-only imports and `(cfg test)` forms. A `cfg test` group
can hold shared helpers and individually marked tests.

The inventory needs no `main`. It visits canonical files once in graph
discovery order and lists each file's tests in declaration order. A success
header gives the count, followed by rows of the form
`test FILE DECLARATION START END NAME PATH`. The declaration index and half-open
byte span refer to the defining source file. A wrong signature reports the
original parameter list or result type. Ordinary native and bootstrap modes
exclude test-only forms and imports. The Stage 1 seed also treats `test`
as false when evaluating `cfg` predicates.

## Inline test execution

Use `--mode test --emit-graph ENTRY SCRATCH CAPACITY (IDENTITY PATH FILE)...`
to emit a test executable from the same supplied graph. The generated
entry uses the discovery order and keeps the ordinary Wove `main` as
an internal function. An empty inventory is valid.

Build the test runtime archive and link it with the emitted C.

```sh
make build/stage-2/test/libwove_test.a
clang -std=c11 -I runtime -I stage-2/test emitted.c build/stage-2/test/libwove_test.a -o ./wove-tests
./wove-tests
```

With no arguments the executable runs each test in a fresh process and
reports status, defining path, function name, and child status in
tab-separated rows. On macOS and Linux, child launches use the running
executable even when the invocation supplies a separate display name.
A failed test leaves later tests runnable and makes
the overall status nonzero. Child stdout is drained and stderr is
inherited. The Wove runner obtains the ordered labels from the generated
entry's `--list` command, which writes an escaped file and name pair per
line. Pass one zero-based decimal test index to run that test directly,
with status 2 for an invalid selection.

## Compile-time file embeds

Use `(embed "./assets/payload.bin")` as an owning local initializer.
The compiler resolves a quoted path relative to the module that contains it.
Absolute paths also work. The result is a fresh owned byte buffer on every
evaluation, with the file's original byte count and content. Emitted C
carries the payload, so the asset can move after compilation.

Only selected expressions request files. A missing, unreadable, or invalid
selected path reports error 67 on the original path token. A trailing slash
or a final `.` or `..` component requires a directory. The graph probe accepts
trailing `--asset VIRTUAL_PATH FILE` triples with raw file bytes.
Asset rows are separate from source modules and imports. Native `cc`
reads disk assets for its explicit source file.

## Native tool capture

[Process capture](io/process.wove) runs one executable and returns its exit
status, the number of bytes written to a caller-owned stdout view, and an
overflow flag. Pass an executable name or path and a packed byte view of
NUL-terminated user arguments. An empty view supplies no user arguments.
Consecutive NULs preserve empty arguments. The executable name or path
becomes `argv[0]` and a bare name uses `PATH`.

The view length is the capture limit. Wove writes only the captured prefix,
drains any excess stdout, and reaps the child. A zero-length view can still
report overflow. The launcher uses close-on-exec descriptors and opens
a private FIFO under `TMPDIR` or `/tmp` when `pipe2` is unavailable. It
removes the FIFO and its directory before launch. Stdin and stderr are
inherited. The status is the normal exit code, 128 plus the terminating
signal, or -1 on host failure. Compile the Wove source with the checked-in
POSIX bridge providers and link the generated C with the Wove runtime.
Enable the GNU declarations when compiling the Linux translation unit.

## Compiler version

`wovec --version` and `wovec version` print
`wovec <major>.<minor>.YYMMDD.HHMM-<git-short-hash>`. The release components
live in [the version module](driver/version.wove). The build metadata uses
the UTC author date of Git `HEAD` and the first seven lowercase hexadecimal
digits of its revision. Git metadata must belong to the Wove source root,
which can be a linked worktree.

Make generates `build/version.txt`, or `version.txt` under the selected
`BUILD_DIR`, before compiling the native entry. Set `WOVEC_BUILD_VERSION`
to a valid `YYMMDD.HHMM-SHA` value for an explicit version or an archive
without Git metadata. Identical content keeps the asset's modification time.
The compiler embeds the generated line and reads its compiled bytes when
printing the version.

## Native source graph compilation

Run `make` to build `build/wovec` or `make stage-2` to stop at
`build/stage-2/wovec`. The linked
executable reads host arguments, resolves command and project settings, and
reads source files through explicit checked-in POSIX C-header imports.

```sh
build/wovec cc --check input.wove other.wove
build/wovec cc input.wove -o build/input
build/wovec bin input.wove -o build/input
build/input
```

The profile's `cc.cstd` selects C11, C17, or C23 for the capability
probes used by both native commands and for the final Clang invocation in
`bin`. The default is C11. The `--cstd` option overrides the profile.
The driver applies the resolved standard after forwarded options at both
the Clang driver and frontend levels.

Ordinary `cc` and `bin` builds take one explicit UTF-8 source path and
require an output target with a basename. `cc --check` accepts one or
more explicit UTF-8 source paths and checks each entry's imports, embeds,
and C availability without creating output. It checks entries in operand
order and continues after an entry fails. Within a disk import graph,
it reports primary import failures in traversal order and continues
checking independent files. A failed shared provider reports once.
Files that depend on a failed input are excluded from subsequent
semantic checks. The command exits successfully only if every entry
passes. A check ignores any supplied `-o`.
The `cc` command and ordinary `bin` builds write
`main.c` below `<target-parent>/.wovec-cache/<profile>/<target-name>/bin/`.
To compile retained C manually when it uses runtime operations, use
`-I runtime` and link `runtime/wove_runtime.c`. The driver reads `wovec.jsonc` from its
working directory for profile selection. `--profile NAME` selects that
profile and uses its name in the retained path. Diagnostics go to stderr
as one JSON object per line by default. Use `--human` or set `human` in
the selected profile to show the path, one-based line and column, and a
caret for the primary source span. `--no-human` selects JSON Lines even
when the profile enables the human view.

The entry can select names from other files with `use wove-file`. The
resolver opens only active edges under the native selection context.
Relative paths resolve from the canonical importing file's directory.
The loader reads a regular file once per device and inode, so diamond,
symlink, and hard-link paths share the same defining source and emitted
functions. An unavailable provider reports the original importing use.
Selected embeds use their defining module's directory. The complete
graph emits as one retained C translation unit.

`bin` launches Clang once with the selected profile's native options and
a private copy of the generated unit containing the packaged runtime.
It uses `clang` from PATH or the executable named by `CLANG`. After Clang
succeeds, it checks that the staged output is a regular executable before
renaming it onto the target. On tool failure it reports the status and
leaves any previous executable in place. Ordinary builds retain optional
Clang side outputs beside the retained C when possible.

A debug `bin` build retains its exact Clang input, bundled runtime, and
optional Clang side outputs in a per-build `.wovec-bin.*` directory under
that same `bin/` cache directory. The debug line table points to the retained C and runtime sources. On
Darwin, load `program.dSYM` from the same directory with LLDB's
`target symbols add` command. A failed rebuild leaves the previous
executable and its retained debug directory intact.

Both commands check the requested target, retained C, and retained tree
against the active inputs before publishing. The check follows existing
file aliases and considers both the original input path and its canonical
ancestors. It covers the entry, transitive imports, embed requests, project
configuration, and selected local bridge headers. Binary builds check
explicit link files before invoking Clang. The same invocation streams
its resolved include graphs separately from the caller's make depfile
settings. The driver checks their paths while the binary and generated
C are still private. A collision reports
code 71 with the conflicting input path and preserves prior artifacts.
An unusable dependency trace reports code 72.

## Structured JSON diagnostics

Use [the diagnostic record](diagnostics/record.wove) to retain an error's
numeric `u32` code and original location. Construct a source location with
`diagnostic_source` and an argument location with `diagnostic_argv`. Source
coordinates are half-open byte offsets in the named file. Argv positions
are zero-based and count from the first user argument after the executable.

Call [`write_diagnostic_json`](diagnostics/json.wove) with the record, its
UTF-8 message, the source name for a source location, a writable output
view, and two bytes of nesting storage. Use the returned `written` prefix
when `error` is zero. The serializer emits one compact object per call.

```jsonl
{"code":24,"message":"Unexpected expression","location":{"kind":"source","file":"example.wove","start":12,"end":24}}
{"code":7,"message":"Unknown option","location":{"kind":"argv","index":2}}
```

Errors 1 and 2 indicate output and nesting capacity. Error 3 indicates an
unknown location tag, and error 4 indicates invalid UTF-8. Discard the
output on failure.

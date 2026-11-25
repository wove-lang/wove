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
double quote, or backtick. Bare atoms have kind 1. Opening and closing
parentheses use kinds 40 and 41. An unquoted semicolon produces a one-byte lexical
error of kind 3. End of input has kind 0 and both offsets equal to the slice
length.

A double-quoted string is kind 2. Its raw span includes both quotes and
preserves the original spelling of JSON short escapes and `\u` escapes with
four hexadecimal digits. A raw byte below `0x20`, an invalid escape, or a
missing quote gives error kind 3. The error starts at the opening quote and
ends just after the offending byte or at the view end on truncation.

Quoted tokens can contain semicolons, including the literal text `;;`.

A raw backtick string also has kind 2. A doubled backtick belongs to the
string and a single backtick closes it. Use the opening byte to distinguish
the string spellings. The span keeps both delimiters, all doubled backticks,
and the original physical line endings. The scanner preserves literal
backslashes, quotes, and semicolons inside backticks. If the view ends
before a closing backtick, the error span starts at the opener and ends at
the view boundary.

The `error` field of a token is zero on success. Error tokens use kind 3
and carry a primary span with a numeric failure code.

| Code | Failure |
| --- | --- |
| 1 | Unexpected delimiter |
| 2 | Unterminated string |
| 3 | Invalid or incomplete escape |
| 4 | Raw control byte in a string |
| 5 | Invalid UTF-8 |

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
admission errors retain codes 1–5. Structural checks use the following codes.

| Code | Failure | Primary span |
| --- | --- | --- |
| 6 | Expected a root form | The first token, or EOF for empty or whitespace-only input |
| 7 | Expected a bare tag | The significant token that followed the opener |
| 8 | Unexpected closer | The closer without a matching opener |
| 9 | Unclosed form | The innermost unmatched opener |
| 10 | Extra input after the root | The first extra significant token |

`build_source_tree` takes the same admitted source view, its successful
`FormCheck`, and a writable `(mut-slice SourceNode)`. It checks capacity
before writing. On success it fills exactly the counted rows in source
order and returns the original check. Short storage returns code 11
with the real root's span.

Row 0 is the synthetic source root and covers the entire view. Node kinds
are 0 for that root, 40 for a parenthesized form, 1 for a bare atom, and 2 for
a quoted atom. Each row carries a half-open byte span and `parent`,
`first_child`, and `next_sibling` indices. An absent link is `-1`. A form
spans both parentheses and has its tag as its first child.

After building the tree, call `check_source_metadata` with the same source
view, successful result, and shared node slice. Form heads use
`[A-Za-z_][A-Za-z0-9_.-]*`. A leading colon marks a metadata entry only
when the form is in its parent's prefix. For a module, function, or module-level
`let` declaration, the bare name directly follows the tag. The contiguous
metadata prefix follows that name.
Other forms use the prefix immediately after the tag. Name slots follow
declaration ancestry through declaration groups and supported C bridges.
An opaque payload can contain the same tags as ordinary data. Each entry
keeps its order, parent link, spelling, and original byte span.

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
`u8`, `i32`, `u32`, and `bool`. Expressions support exact-type literals, binding reads,
and direct calls. A local declared with `let` or `let-mut` takes its
scalar type from its initializer and comes into scope afterwards. A `set`
requires an existing `let-mut` binding and a value of that binding's exact
type. Forward calls use checked prototypes, and nested calls evaluate from
left to right.

An immutable module binding takes a typed integer literal or a quoted
string. Its name precedes metadata.

```lisp
(let lowest_digit (i8 -1))
(let alphabet "0123456789abcdef")
```

Functions use `(local lowest_digit)` to read the scalar after declaring
or selecting the binding. A local or parameter with the same name takes
precedence. A quoted string is a shared `u8` view over its decoded
bytes. Generated C puts a referenced nonempty byte table in a single read-only array
with no added terminator. The ordinary `slice.len`, `slice.load`, and
`borrow-shared` forms work with the static view.

A helper with `(returns void)` can use `(return)` or finish at the end
of its body. Invoke it with a standalone `(call ...)` statement. The
call checks its arguments, and either exit releases the helper's live
local owners.

An executable's `main` returns `i32`. A parameterless entry uses the ABI
`int main(void)`. In native mode, `main` may receive a `(shared-slice u8)`
followed by a `(shared-slice ArgSpan)`. The span record has `start` and `end`
fields of type `i32` in that order. A type alias retains the defining record
identity for calls to the [typed argv parser](cli/argv.wove).

The native argv wrapper receives `int argc, char **argv`. It skips the
executable name and copies the original argument bytes without their C
terminators. Each user argument has a half-open span in source order.
The first row is index zero. Empty arguments have equal endpoints, and
zero user arguments supply two empty views. Wove borrows both views for
the entry call. The wrapper releases the backing storage on return.

A C bridge can expose fields of a native record with
`(struct Stat (c-type "struct stat") (fields (st_size Offset)))`.
Select its type and the relevant function with `use c-header`.
`(let-mut info (record.make Stat))` zero-initializes storage of the
header's complete type with its native alignment. A `(mut-ref Stat)`
C parameter takes `(borrow-mut (local info))` for the duration of the
call. A `(shared-ref Stat)` takes a shared borrow and passes a const
pointer. Subsequent `(record.get (local info) st_size)` reads the
native field with its declared Wove type. The C header verifies
the field spelling and type, including the platform-specific
timestamp members in the bundled Stat views.

A module-local `(struct Token (fields (kind u32) (start i32)))` defines
a named, copyable record. Fields can use `bool`, `i8`, `u8`, `i32`, or `u32`.
`(record.make Token (kind (u32 40)) (start (i32 2)))` evaluates each
initializer exactly once in the order written. Supply every field once
with its declared type. A local stores the result by value, and
`(record.get (local tok) start)` reads a field with its exact type.

Functions can take and return locally declared or explicitly imported records.
Calls pass these values by copy, and a caller retains its original local.
An argument or return must match the defining record identity, so two
types with the same fields remain distinct. Record layout declarations
can follow the signatures that refer to them.

`(bool true)` and `(bool false)` are scalar literals.
`i32.eq` and `i32.lt` compare signed operands. `u8.eq`, `u8.lt`,
`u32.eq`, and `u32.lt` compare unsigned operands. Each comparison
requires operands of its named scalar type and returns `bool`.
`(int.eq Type left right)` also compares two integers of the exact
named type, including native aliases such as `Device` and `Inode`.
`i32.add.wrapping` and `i32.sub.wrapping` take `i32` operands and
return their signed 32-bit result modulo 2^32. Each binary operand is
evaluated once, left to right. Emitted C widens the arithmetic,
reduces it to 32 bits as unsigned, and produces an in-range signed result.

`u32.add.wrapping` and `u32.sub.wrapping` take exact `u32` operands
and yield `u32` results modulo 2^32. Each operand runs once, left to
right. Emitted C widens the operands to unsigned 64-bit arithmetic and
keeps the low 32 bits.

`u32.and` and `u32.or` combine two exact `u32` operands bitwise.
`(u32.shl word count)` and `(u32.shr word count)` take an exact
`u32` word and `i32` count. The left shift discards bits above the
word width, and the logical right shift fills with zeroes. All four
forms return `u32` and evaluate both operands once, left to right.
A count outside `[0, 32)` traps before shifting and reports the
original file ID and half-open span of the shift.

`(int.convert target value)` evaluates the integer operand once and
preserves its numerical value when it fits the target `i8`, `u8`, `i32`,
or `u32`. A value outside the target range traps before the cast. The
runtime diagnostic identifies the original file ID and half-open byte
span of the conversion, including from a source subrange or an imported
file. C emitted for conversions includes `wove_runtime.h` and links
with the shared Wove C runtime.

Helpers can take `(shared-slice u8)` parameters using the runtime's
`wvrt_slice` ABI. Pass a named view to another helper with
`(borrow-shared (local data))`. `(slice.len (local data))` returns its
signed `i32` length. `(slice.load (local data) index)` takes an exact
`i32` index and returns a `u8`. A negative index or an index at or beyond
the length traps before reading the slice, with the original file ID and
half-open byte span of the load. C emitted for a shared view includes
`wove_runtime.h`. Link the shared runtime when the code uses a checked load.

A `(shared-slice Token)` parameter can refer to a named, copyable scalar
record. Its C descriptor is also a `wvrt_slice`. Pass a whole view to
another function with `(borrow-shared (local data))` and the same defining
element type. `slice.len` counts records and `slice.load` checks its
`i32` index before copying the selected record by value. A C caller
supplies the original array of the emitted record type and its element
count and size.

Helpers can also take `(mut-slice u8)` parameters using `wvrt_mut_slice`.
Pass one to a writable parameter with `(borrow-mut (local data))`, or
to a shared parameter with `(borrow-shared (local data))`. An explicit
reborrow lasts through the call. A later argument cannot use the same
parameter while it is exclusively borrowed, and a shared loan prevents
an exclusive one until the call returns. Both view types support
`slice.len` and `slice.load`.

`(slice.store (local data) index value)` is a statement. It requires a
mutable byte view, an exact `i32` index, and an exact `u8` value. It
evaluates its operands in order and checks bounds before writing the
byte in the caller's storage. An out-of-bounds store reports the same
file identity and half-open span format as a checked load.

A `(mut-slice Token)` carries the same defining record type as its
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

Use `(slice.store (local output) index value)` to fill a local byte array
bound with `let-mut` before returning it by move. The store holds an
exclusive view of the live owner while evaluating the index and value,
then checks the index and writes the byte. A call in either operand
can borrow a different buffer. The index and value have the same exact
`i32` and `u8` types and bounds diagnostics as a store through a slice
parameter.

For a copyable record, `(array.alloc Token count)` owns `count` zeroed
records. The runtime allocates with `sizeof` and `_Alignof` of the
defining C type. A whole-array call borrow uses a slice of the same
defining record and the allocation's original storage. A `let-mut`
binding permits exclusive access. The usual scope exits and moves apply
to record owners.

A shared call argument can use
`(borrow-shared (slice.range (local source) start end))` with a named byte
owner or shared byte slice. The source view and its exact `i32` endpoints
are evaluated once, left to right. The runtime checks
`0 <= start <= end <= source.len` before entering the callee. The view
uses the original storage, with length `end - start` and index zero at
`start`. The loan lasts from projection through the call, including
later argument evaluations. A bounds failure reports the range's span.

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

A Wove helper can declare `(returns (owned-array u8))` and transfer an
array with `(return (move (local data)))`. This transfers the original
allocation on that exit while releasing the other live owners. An
alternate path still sees the local until it transfers or releases it.
A wrapper can also return the fresh owner from a call directly. The
caller binds the result with `let` or `let-mut` and can read, borrow, or
move it through its normal owner lifetime. A zero-length result follows
the same ownership rules.

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
test. A return inside the loop exits the function. A loop can skip its
body, so a value-returning function still needs a return on the path
after the loop.

[FNV-1a](hashing/fnv1a.wove) exposes the 31-bit seed with `fnv1a_seed`
and updates it one raw byte at a time with `fnv1a_byte`. The declaration
identity builder supplies the order and delimiters for its components.

[Base32hex](hashing/base32hex.wove) exposes `base32hex31_width` and
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

A `use c-header` selects functions and native types from a `bridge c-header` with the same
decoded header key in the supplied source table. A key such as `"<stdlib.h>"`
or `"./project.h"` identifies one active bridge in a well-formed canonical file.
Header lookup leaves Wove-file reachability unchanged. A selected declaration
has `params` and `returns` forms. Both support exact `bool`, `u8`, `i32`, and
`u32` types. Shared and mutable byte-slice parameters require an explicit
native argument contract. Parameter names must be unique within the signature. The alias occupies the
importing file's function namespace. Its binding keeps the defining declaration
and the raw header, native symbol, and signature spans in that file.

The scalar graph compiler checks each C call against the selected Wove
signature and evaluates its arguments once from left to right. The emitted
call uses the original native name. Each selected header contributes one
include operand. Angle-bracket keys use system search. Bare or explicitly
quoted project paths use quoted includes. The C compiler reads the native
declarations from those headers.

Use `(:pragma (c_cstr parameter ...))` or `(:pragma (c_cstr_raw parameter ...))`
after the bridge function name to pass one or more shared byte slices as
`const char *`. After evaluating all arguments, the call copies each named
slice and appends a NUL byte. Checked mode traps at the Wove argument span
on an embedded NUL before calling C. Raw mode copies every byte, including
bytes after an embedded NUL. An empty view passes a pointer to one NUL byte.
C can read the temporary buffers until the call returns, when Wove releases
their backing storage.

Use `(:pragma (c_bytes data count))` for a byte count or
`(:pragma (c_bytes data size count))` for a product. The named dimensions
accept `u8`, `i32`, or `u32` formals. After argument evaluation, both
dimensions must be nonnegative and their extent must fit in the selected
view length. Stage 2 compares by division to avoid overflow. Zero extents
can lend empty views. The check traps at the data argument before entering
C. A shared view passes a read-only pointer, and a mutable view passes a
writable pointer to the same storage for the duration of the call.

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

`make stage-2` translates [the compiler root](wovec.wove) with the Python seed
and links `build/stage-1/wovec` with Clang and the shared runtime. The root
imports `admit_source` and `scan_token` from [the scanner](source/scan.wove).
Its parameterless bootstrap entry admits an empty view and expects an EOF
token.

The seed translates a separate [Wove fixture root](../tests/stage-2/source_scan.wove)
for the [C source probe](../tests/stage-2/source_scan.probe.c). The probe also
supplies graph emission when building the native compiler. Run `make test-stage-2`
to run the bootstrap entry and compare scanner streams with the Stage 1 lexer. The
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

Link the emitted C with the Wove runtime and test runner.

```sh
clang -std=c11 -I runtime -I stage-2/test emitted.c runtime/wove_runtime.c \
	stage-2/test/wove_test.c stage-2/io/process.c -o ./wove-tests
./wove-tests
```

With no arguments the executable runs each test in a fresh process and
reports status, defining path, function name, and child status in
tab-separated rows. On macOS and Linux, child launches use the running
executable even when the invocation supplies a separate display name.
A failed test leaves later tests runnable and makes
the overall status nonzero. Child stdout is drained and stderr is
inherited. Pass one zero-based decimal test index to run that test
directly, with status 2 for an invalid selection.

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

The view length is the capture limit. The host bridge writes only the
captured prefix, drains any excess stdout, and reaps the child. A zero-length
view can still report overflow. On hosts without `pipe2`, the bridge opens
a private FIFO under `TMPDIR` or `/tmp` and removes it before launch. Stdin
and stderr are inherited. The status is the normal exit code, 128 plus the
terminating signal, or -1 on host failure. Consumers link
[the POSIX bridge](io/process.c) and make its header available to the
generated C translation unit.

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

## Native single-source compilation

Run `make stage-2` to build `build/stage-2/wovec`. The seed-built Stage 2
emitter compiles the native entry and its Wove dependencies. The linked
executable reads host arguments, resolves command and project settings, and
uses its C bridges for file I/O.

```sh
build/stage-2/wovec cc input.wove -o build/input
clang -std=c11 build/.wovec-cache/default/input/bin/main.c -o build/input
```

`cc` takes one explicit UTF-8 source path and an explicit output target
with a basename. It writes
`main.c` below `<target-parent>/.wovec-cache/<profile>/<target-name>/bin/`.
For output that uses Wove runtime operations, compile with `-I runtime`
and link `runtime/wove_runtime.c`. The command reads `wovec.jsonc` from its
working directory for profile selection. `--profile NAME` selects that
profile and uses its name in the retained path. Argument and source
failures print one JSON diagnostic to stderr. On-disk source graph loading
is tracked in [#188](https://github.com/wove-lang/wove/issues/188).

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

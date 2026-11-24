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
when the form is in its parent's prefix. That prefix is the contiguous
sequence of colon-tagged forms immediately after the parent's tag. The
first ordinary child after the prefix is operand zero. Each entry keeps its
order, parent link, spelling, and original byte span.

| Code | Failure | Primary span |
| --- | --- | --- |
| 12 | Invalid head spelling | The head atom |
| 13 | Misplaced metadata entry | The colon-prefixed head atom |
| 14 | Retired `with-meta` or `meta` form | The head atom |

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
checked source tree. `form_parts` returns the head plus the first metadata
and first ordinary operand indices. An empty view uses `-1`. Walk metadata
siblings up to the first ordinary operand and walk ordinary operands to the
end. All indices refer to existing tree rows.

`read_module` reads the name from the first ordinary operand and writes the
original indices of selected declarations into a caller-provided record slice.
It applies bootstrap `cfg` and `test` selection to the module, its groups, and
its declarations. It flattens active `decls` in source order. An excluded
module succeeds with its name and zero declarations. Unknown metadata and
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

The [C emitter](emit/c.wove) consumes a checked tree, its module result,
and the selected declaration indices. It checks scalar functions that
return a value on every path. Helpers can accept and return `i32`, `u8`,
`u32`, and `bool`. Expressions support exact-type literals, binding reads,
and direct calls. A local declared with `let` or `let-mut` takes its
`i32`, `u8`, or `u32` type from its initializer and comes into scope
afterwards. A `set` requires an existing `let-mut` binding and a value
of that binding's exact type. The selected `main`
has the ABI signature `int main(void)`. Forward calls use checked
prototypes, and nested calls evaluate from left to right.

A module-local `(struct Token (fields (kind u32) (start i32)))` defines
a named, copyable record. Fields can use `bool`, `u8`, `i32`, or `u32`.
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
`i32.add.wrapping` and `i32.sub.wrapping` take `i32` operands and
return their signed 32-bit result modulo 2^32. Each binary operand is
evaluated once, left to right. Emitted C widens the arithmetic,
reduces it to 32 bits as unsigned, and produces an in-range signed result.

`u32.add.wrapping` and `u32.sub.wrapping` take exact `u32` operands
and yield `u32` results modulo 2^32. Each operand runs once, left to
right. Emitted C widens the operands to unsigned 64-bit arithmetic and
keeps the low 32 bits.

`(int.convert target value)` evaluates the integer operand once and
preserves its numerical value when it fits the target `u8`, `i32`, or
`u32`. A value outside the target range traps before the cast. The
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

A function-local `let` or `let-mut` can own `(array.alloc u8 length)`.
The length is an exact `i32`. The runtime allocates zeroed bytes and
reports an invalid length or allocation failure at the allocation's
original file identity and byte span. A shared call borrow keeps the
owner's original storage. A mutable call borrow requires `let-mut`.

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
and the source name's byte span. The seed represents the role enum with
a record and named constructors. The generator writes
`wg<role><id><suffix>`, where `wg` stands for Wove-generated and the
third character identifies the role. The ID uses seven lowercase
Base32hex digits and starts with `0` or `1` because IDs are 31 bits.
A suffix keeps ASCII letters, digits, and underscores. Other bytes
become underscores.

| Tag | Role | ID | Suffix |
| --- | --- | --- | --- |
| `f` | Function | Stable declaration hash | Function name |
| `r` | Record | Stable declaration hash | Record name |
| `m` | Record member | Zero-based position in its layout | Empty |
| `p` | Parameter | Zero-based position in its signature | Parameter name |
| `l` | Local | Declared name's original file-local tree row | Local name |
| `t` | Expression temporary | Original expression's file-local tree row | Empty |

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
| 23 | Unsupported signature or record field type | The parameter list, result, field type, or offending signature form |
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
and binds active function and record imports in separate namespaces. The table maps
normalized absolute paths to defining file identities and source ranges in a
shared byte arena. Relative imports use the directory of the reached path.
Stage 2 resolves virtual paths lexically before lookup, so a directory
discarded by `..` can be absent from the graph. Stage 1 requires each
intermediate directory to exist on the host filesystem.
Several paths can refer to one identity, which the resolver reads once.
An empty `only` still visits its provider. An excluded module contributes
no names or nested imports.

In an `only` list, `Token` or `(as Token read_token)` selects a
function. `(type Token)` or `(type (as Token LocalToken))` selects a
record. The same name can refer to one function and one type in a file.
Each selector binds a declaration defined by the provider.

Each binding identifies the importing namespace and the name span in that
file. Its target is the defining file identity and declaration node index
in the defining source tree. Earlier reader diagnostics keep their codes
and add the identity of the file where they originated.

| Code | Failure | Primary span |
| --- | --- | --- |
| 16 | Short graph storage | The next row's source node, when available |
| 26 | Malformed use, selector, or declaration name | The originating form or operand |
| 27 | Missing active provider | The importing use |
| 28 | Active import cycle | The closing use |
| 29 | Unavailable function or type | The requested name |
| 30 | Conflicting binding | The new local name |
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

The [scanner fixture](../tests/stage-2/source_scan.probe.c) uses the same
generated C. Run `make test-stage-2` to run the bootstrap entry and compare
scanner streams with the Stage 1 lexer. The
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

Use `--module FILE [START END [DECL_CAPACITY]]` to select a checked module.
The first row contains the selected count, start, end, error, line, and
column. The second row gives the name node index and the active flag.
When the module has a checked name, the remaining rows describe the module
and the selected declarations. Each lists the original form, head, first
metadata, and first ordinary operand indices. The
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
and `type-binding` for records. A failure gives the code and byte span with
one-based coordinates in its original file.

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

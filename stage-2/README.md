<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 2 compiler

The self-hosted compiler lives in this directory. Its sources use the
[bootstrap language subset](LANGUAGE-SUBSET.md). For application code,
use the [Wove language reference](../LANGUAGE.md).

## Source scanner, tree reader, and renderer

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

After building the tree, call `check_source_metadata` with the original source
view, successful check result, and shared node slice. Each metadata entry
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

Semantic passes skip `disabled` groups. The checked tree and renderer preserve
their original nodes and metadata.

## C emission and source graph

The [C emitter](emit/c.wove) consumes a checked tree, its module result,
and the selected declaration indices.

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

## Bootstrap build and compiler fixtures

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
with the supplied-graph probe to discover tests.

The inventory needs no `main`. It visits canonical files once in graph
discovery order and lists each file's tests in declaration order. A success
header gives the count, followed by rows of the form
`test FILE DECLARATION START END NAME PATH`. The declaration index and half-open
byte span refer to the defining source file. A wrong signature reports the
original parameter list or result type. Ordinary native and bootstrap modes

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

Only selected expressions request files. A missing, unreadable, or invalid
selected path reports error 67 on the original path token. The graph probe
accepts trailing `--asset VIRTUAL_PATH FILE` triples with raw file bytes.
Asset rows are separate from source modules and imports.

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

The native build commands are `cc` and `bin`. Both build one translation
unit at a time. A project can omit `parallel` or set it to `false`.
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

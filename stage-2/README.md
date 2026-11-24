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

Use `decode_string` from [the string decoder](source_strings.wove) with the
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

The [module reader](source_module.wove) works from the original bytes and a
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

The [C emitter](emit_c.wove) consumes a checked tree, its module result,
and the selected declaration indices. `emit_module` accepts one selected
`main` with empty `params`, an `i32` result, and one `return` of an
`(i32 ...)` literal. Its output is a complete C translation unit with the
ABI entry `int main(void)`. It checks the exact signed decimal range and
emits significant digits, so source leading zeros keep their decimal value.

The result gives the written count and a view-relative span. Successful
emission spans the module. A failure preserves the destination bytes and
uses the originating span. Tree and module diagnostics keep their codes.

| Code | Failure | Primary span |
| --- | --- | --- |
| 16 | Short C output view | The selected entry |
| 22 | Expected one selected main | The module, unexpected declaration or name, or extra active declaration |
| 23 | Unsupported entry signature | The function, parameter list, or return type |
| 24 | Unsupported body or expression | The originating body form, statement, or expression |
| 25 | Invalid signed decimal i32 | The literal value or malformed literal form |

The [graph resolver](source_graph.wove) reads a supplied virtual file table
and binds active function imports in each file's namespace. The table maps
normalized absolute paths to defining file identities and source ranges in a
shared byte arena. Relative imports use the directory of the reached path.
Stage 2 resolves virtual paths lexically before lookup, so a directory
discarded by `..` can be absent from the graph. Stage 1 requires each
intermediate directory to exist on the host filesystem.
Several paths can refer to one identity, which the resolver reads once.
An empty `only` still visits its provider. An excluded module contributes
no names or nested imports.

Each binding identifies the importing namespace and the name span in that
file. Its target is the defining file identity and function's node index
in the defining source tree. Earlier reader diagnostics keep their codes
and add the identity of the file where they originated.

| Code | Failure | Primary span |
| --- | --- | --- |
| 16 | Short graph storage | The next row's source node, when available |
| 26 | Malformed use, selector, or function name | The originating form or operand |
| 27 | Missing active provider | The importing use |
| 28 | Active import cycle | The closing use |
| 29 | Unavailable function | The requested name |
| 30 | Conflicting binding | The new local name |
| 31 | Excluded entry module | The entry module |

`make stage-2` translates [the compiler root](wovec.wove) with the Python seed
and links `build/stage-1/wovec` with Clang and the shared runtime. The root
imports `admit_source` and `scan_token` from [the scanner](source_scan.wove).
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

Use `--emit-c FILE [START END [OUTPUT_CAPACITY]]` to compile the scalar entry.
The first row gives the written count, source span, error, line, and column.
The remaining bytes are the C translation unit. The
[compilation fixtures](../tests/stage-2/source_compilations.tsv) compile the
emitted C with Clang, run the executable returning 42, and check the full
return values at the i32 boundaries.

The [graph fixtures](../tests/stage-2/source_graphs.tsv) pass virtual paths
and raw source files to `--graph ENTRY CAPACITY (IDENTITY PATH FILE)...`.
The probe prints the reached module rows, local bindings, and their defining
identities and node indices. A failure gives the code and byte span with
one-based coordinates in its original file.

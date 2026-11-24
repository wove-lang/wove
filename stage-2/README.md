<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 2 source scanner, tree reader, and renderer

Call `source_diagnostic` once on the shared byte slice before tokenization.
The admission check covers the entire view, including bytes in comments and
strings. Pass the same bytes to `scan_token` after a successful check.

`scan_token` reads one token from a shared byte slice. Start with an `i32` cursor
of zero and use each token's `end` as the next cursor. A cursor from zero through
the slice length is valid. The runtime rejects cursors outside that range.

Each `Token` carries a `u32` kind and an `i32` half-open span, `[start, end)`,
relative to the supplied slice. The scanner uses the seed's whitespace set
for both skipping separators and ending bare atoms. Separators consume their
UTF-8 byte widths. A bare atom also stops at a parenthesis, a semicolon, or a
double quote. Atom kind is 1, opening and closing parenthesis
kinds are 40 and 41, and a lone semicolon consumes one byte with lexical error
kind 3. End of input has kind 0 and both offsets equal to the slice length.

A double-quoted string is kind 2. Its raw span includes both quotes and
preserves the original spelling of JSON short escapes and `\u` escapes with
four hexadecimal digits. A raw byte below `0x20`, an invalid escape, or a
missing quote gives error kind 3. The error starts at the opening quote and
ends just after the offending byte or at the view end on truncation.

A `;;` line comment is a raw token of kind 59. Its span includes both
semicolons and all comment text up to CR, LF, or the end of the view. The
line ending is available to the next scan as whitespace, and offsets
preserve the original bytes under LF, CRLF, and lone CR. A caller can skip
kind 59 to walk the significant tokens.

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
requires a bare tag as the first significant token. Operands may be atoms,
quoted strings, or nested forms. Comments remain in source order and count
toward the row total.

The result gives the real root's span, including its closing parenthesis,
and the number of rows needed for the source tree. Count one synthetic source
root and one row for each opener, bare atom, quoted atom, or comment.

A failed check returns zero rows and the original diagnostic span. Lexer and
admission errors retain codes 1–5. Structural checks use the following codes.

| Code | Failure | Primary span |
| --- | --- | --- |
| 6 | Expected a root form | The first significant token, or EOF for empty or comment-only input |
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
are 0 for that root, 40 for a parenthesized form, 1 for a bare atom, 2 for a
quoted atom, and 59 for a comment. Each row carries a half-open byte span
and `parent`, `first_child`, and `next_sibling` indices. An absent link is
`-1`. A form spans both parentheses. Its tag is the first child after
skipping any preceding comment rows. Atom and comment spellings are
available from the original source bytes.

After building the tree, use `check_source_metadata` with the same source
view, successful result, and shared node slice. Form heads use
`[A-Za-z_][A-Za-z0-9_.-]*`. A leading colon marks an entry head only in
the direct `meta` list operand of a `with-meta` wrapper.

The significant operands of `with-meta` are one `meta` list followed by one
target. Each significant entry in the list is a colon-tagged form. The
target is the next significant sibling of the list. Comments and extension
entries keep their original rows, links, and byte spans.

| Code | Failure | Primary span |
| --- | --- | --- |
| 12 | Invalid head spelling | The head atom |
| 13 | Misplaced metadata head | The colon-prefixed head atom |
| 14 | Malformed wrapper | The offending operand or the wrapper itself when an operand is missing |
| 15 | Malformed metadata entry | The offending direct entry |

`render_source_tree` takes the original byte view, its built and validated
shared node slice, and a writable byte slice. It walks the tree links in
source order and copies each token's raw bytes, including parentheses,
quoted escapes, and comments. It adds one space between tokens except
immediately inside parentheses, and uses LF after a comment whenever another
token follows. Layout around the outermost tokens is discarded.

The result contains the number of bytes written and an error code. Code 0
means the emitted prefix contains the entire source. Code 16 means the next
token and its separator could not fit. In either case the prefix ends on a
token boundary and the remaining output bytes retain their original values.

`make stage-2` emits C with the Python seed and links the scanner with
its [source fixture](../tests/stage-2/source_scan.probe.c) and the shared
runtime. Run `make test-stage-2` to exercise those inputs and compare full
streams with the Stage 1 lexer. The [layout fixtures](../tests/stage-2/lexer_layouts.tsv)
supply indentation, line endings, and parent view bytes. The parity check
compares the resulting token kinds and spellings, including comments.
`make test` includes these checks.

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

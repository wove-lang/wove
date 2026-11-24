<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 2 source scanner

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

`make stage-2` emits C with the Python seed and links the scanner with
its [source fixture](../tests/stage-2/source_scan.probe.c) and the shared
runtime. Run `make test-stage-2` to exercise those inputs and compare full
streams with the Stage 1 lexer. The parity check also renders a fixture
with different indentation and line endings and checks its token kinds
and spellings, including comments. `make test` includes these checks.

The seed-built fixture executable accepts `--tokens FILE [START END]` to
print numeric rows with kind, start, end, error, line, and column. The span
is relative to the selected view and the coordinates use the whole file.

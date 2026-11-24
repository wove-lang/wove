<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 2 source scanner

`scan_token` reads one token from a shared byte slice. Start with an `i32` cursor
of zero and use each token's `end` as the next cursor. A cursor from zero through
the slice length is valid. The runtime rejects cursors outside that range.

Each `Token` carries a `u32` kind and an `i32` half-open span, `[start, end)`,
relative to the supplied slice. The scanner skips ASCII space, tab, carriage
return, and newline. A bare atom stops at whitespace, a parenthesis, a
semicolon, or a double quote. Atom kind is 1, opening and closing parenthesis
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

The first-byte helpers `source_offset` and `source_kind` find the first
significant byte from cursor zero. `source_kind` returns 0 at EOF, 40 or 41
for a parenthesis, and 1 for every other leading byte.

`make stage-2` emits C with the Python seed and links the scanner with
its [source fixture](../tests/stage-2/source_scan.probe.c) and the shared
runtime. Run `make test-stage-2` to exercise those in-memory inputs.
`make test` includes the scanner fixtures.

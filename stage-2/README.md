<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 2 source scanner

`source_scan.wove` finds the first significant byte of a shared source slice.
`source_offset` returns its `i32` offset relative to the view. `source_kind`
returns `u32` kind 0 at the end, 40 or 41 for a parenthesis, and 1 for any
other leading byte. The scanner skips ASCII space, tab, carriage return, and
newline.

`make stage-2` emits C with the Python seed and links the scanner with
its [source fixture](../tests/stage-2/source_scan.probe.c) and the shared
runtime. Run `make test-stage-2` to exercise those in-memory inputs.
`make test` includes the scanner fixtures.

<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 1 seed

The Python seed reads one module of tagged IR forms and emits C11 to stdout.
The subset covers functions that return `u8`, `i32`, `u32`, `bool`, or
`void`, owned byte arrays, and call-scoped shared and exclusive slice
borrows. Every path through a scalar function returns a value. A void
function can return without a value or complete at the end of its body.
A source rejection prints the filename and line.

Write integer literals as typed decimal nodes such as `(u32 4294967295)`.
The seed checks the range of each literal and preserves its exact type
through local bindings, parameters, calls, and returns. Use
`(int.convert i32 (local byte))` to explicitly convert between seed integers.
The conversion preserves values that fit and traps at the source line when a
value is outside the selected type's range.

`i32.add.wrapping`, `i32.sub.wrapping`, `u32.add.wrapping`, and
`u32.sub.wrapping` compute modulo 2^32. Both operands and the result have
the operation's named type.

Write boolean literals as `(bool true)` or `(bool false)`. Equality and
less-than forms such as `(u8.eq (local byte) (u8 40))` and `(i32.lt (i32 -1)
(i32 0))` take two operands of the named integer type and return `bool`.

An `if` statement takes a boolean condition, a `(then ...)` block, and an
optional `(else ...)` block. Only the selected block executes. Each arm
releases its local owners on exit, and a return releases all enclosing scopes.
An outer binding is available after the branch when every continuing path
still owns it. An omitted `else` contributes the incoming state.
The seed rejects statements after a return or a branch whose arms both return.

`slice.load` reads a `u8` from either kind of byte slice. `slice.store` needs
a mutable byte slice and writes to the owner's storage. Both take an `i32`
index and trap if it falls outside the slice.

Run `make stage-1` to translate and link the ownership example with Clang.
The resulting `build/stage-1/ownership.transfer` exits with status 6.
Run `make test` to check the transfer, its allocation lifetime, and the borrow
rejection cases. The runner reads fixture paths and expected results from
[the case manifest](../tests/stage-1/cases.tsv). Each row specifies a mode,
a source path relative to the manifest, and an exit status or diagnostic line.
Trap cases match the runtime diagnostic at that line.
Use decimal numbers without leading zeros for expected results.
Tracked cases also run with the allocation probe.

The seed entry point is `python3 stage-1/wovec.py SOURCE`. The generated C
includes `wove_runtime.h` and links with `runtime/wove_runtime.c`. The byte
array implementation and slice layouts come from the [archived C runtime](https://github.com/wove-lang/.0.wove/tree/8ac13c06c422a2e2e9c66f59ec1a54f1fe106e52/runtime).

The [ownership expectation](https://github.com/wove-lang/wove/issues/1) stays
on the function in the IR. Stage 2 will provide the structured diagnostic
and related move location. The fixture IDs in the IR remain metadata.

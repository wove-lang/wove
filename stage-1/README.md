# Stage 1 seed

The Python seed reads one module of tagged IR forms and emits C11 to stdout.
The first subset covers straight-line functions with explicit returns, `i32`,
owned byte arrays, and call-scoped shared and exclusive slice borrows.
A source rejection prints the filename and line.

Run `make stage-1` to translate and link the ownership example with Clang.
The resulting `build/stage-1/ownership.transfer` exits with status 6.
Run `make test` to check the transfer, its allocation lifetime, and the borrow
rejection cases. The runner reads fixture paths and expected results from
[the case manifest](../tests/stage-1/cases.tsv). Each row specifies a mode,
a source path relative to the manifest, and an exit status or rejection line.
Tracked cases also run with the allocation probe.

The seed entry point is `python3 stage-1/wovec.py SOURCE`. The generated C
includes `wove_runtime.h` and links with `runtime/wove_runtime.c`. The byte
array implementation and slice layouts come from the [archived C runtime](https://github.com/wove-lang/.0.wove/tree/8ac13c06c422a2e2e9c66f59ec1a54f1fe106e52/runtime).

The [ownership expectation](https://github.com/wove-lang/wove/issues/1) stays
on the function in the IR. Stage 2 will provide the structured diagnostic
and related move location. The fixture IDs in the IR remain metadata.

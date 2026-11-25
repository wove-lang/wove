<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 1 seed

The Python seed is an internal build tool for bootstrapping Stage 2.
It reads an entry module and its local imports and emits one C11 translation unit to stdout.
The subset covers scalar and record values, owned arrays, and call-scoped
shared and exclusive slice borrows. Every path through a function with a result
type returns a value. A `void` function can return without a value or complete
at the end of its body.
A source rejection prints the filename and line.

Write `i8`, `u8`, `i32`, and `u32` literals as typed decimal nodes such as
`(i8 -128)` or `(u32 4294967295)`. The seed checks the range of each
literal and preserves its exact type through bindings, parameters, calls,
and returns. Use
`(int.convert i32 (local byte))` to explicitly convert between seed integers.
The conversion preserves values that fit and traps at the source line when a
value is outside the selected type's range.

`i32.add.wrapping`, `i32.sub.wrapping`, `u32.add.wrapping`, and
`u32.sub.wrapping` compute modulo 2^32. Both operands and the result have
the operation's named type.

Write boolean literals as `(bool true)` or `(bool false)`. Equality and
less-than forms such as `(u8.eq (local byte) (u8 40))` and `(i32.lt (i32 -1)
(i32 0))` take two operands of the named integer type and return `bool`.

`(bool.and left right)` and `(bool.or left right)` evaluate the left
operand first. They evaluate the right operand once when the left operand is
true for `bool.and` or false for `bool.or`. The seed type-checks both operands
as `bool` in every case. `(bool.not value)` negates one boolean. All three
operations return `bool`.

Declare a named record with `(struct Token (fields (kind u32) (start i32)
(end i32)))`. Its fields can have type `bool`, `i8`, `u8`, `i32`, or `u32`.
A `record.make` names the record type and supplies each field once, using its
declared type. Initializer expressions run in the order written. Use
`(record.get (local token) start)` to read a field as its declared scalar type.
Bindings, arguments, and returns copy records by value and preserve the
record's defining declaration as its type.

An `if` statement takes a boolean condition, a `(then ...)` block, and an
optional `(else ...)` block. Only the selected block executes. Each arm
releases its local owners on exit, and a return releases all enclosing scopes.
An outer binding is available after the branch when every continuing path
still owns it. An omitted `else` contributes the incoming state.
The seed rejects statements after a return or a branch whose arms both return.

A `(while condition (body ...))` evaluates its boolean condition before
each iteration. The body has its own scope on each pass. Outer owners
available on entry must still be available on every path back to the
condition. A return runs the usual cleanup for every scope it leaves.
Use `(set (local name) value)` to assign an existing `let-mut` scalar.
The new value has the same type as the binding.

Allocate an owned buffer with `(array.alloc Token count)` for a declared
copyable record, or use `u8` for a byte buffer. A whole-buffer loan preserves
the buffer's element type in `(shared-slice Token)` or `(mut-slice Token)`.
`slice.len` counts elements. `slice.load` copies one element by value.
`slice.store` requires a mutable view and the exact declared element type.
Both take an `i32` element index and trap before an out-of-bounds access.

Use `borrow-shared` to forward either kind of slice to a helper.
Use `borrow-mut` to forward a mutable slice exclusively. A reborrow
uses the original storage, and its loan stays active through later
arguments and the call. Shared loans permit reads of the parent.
Exclusive loans suspend access to the parent until the helper returns.

Use `(borrow-shared (slice.range (local source) start end))` to pass a
half-open range of a named byte buffer or slice. The seed evaluates the
`i32` endpoints once from left to right, then checks
`0 <= start <= end <= source.len`. The helper sees the original
storage starting at `start` and a length of `end - start`.
Equal endpoints form an empty view, including at the end of the source.

A module-level `let` declares an immutable binding in its defining file.
It takes a typed integer literal or a quoted string as its initializer.

```lisp
(let lowest_digit (i8 -1))
(let base16_alphabet "0123456789abcdef")
```

Use `(local lowest_digit)` to read the scalar in a function. A local
binding or parameter with the same name takes precedence. An imported
function reads its own file's bindings, and `use` selects functions and
types in the seed.

A string initializes a shared `u8` slice backed by one static read-only
table. Its length counts decoded UTF-8 bytes, with no added NUL. Use
`(slice.load (local base16_alphabet) (local index))` with the ordinary
slice bounds checks. The same place works with `slice.len` and
`borrow-shared`. Attempts to write a module binding fail at the source
operation.

A `module`, `fn`, or module-level `let` declaration starts with its tag
and bare name. Place its contiguous metadata prefix after the name and
before ordinary fields, as in `(fn main (:doc "Return success.") (params) ...)`.
The same naming syntax applies to functions in C bridges.
Declaration groups carry that context through their members. Opaque
metadata payloads can use the same form tags as ordinary data.

Other forms keep metadata immediately after the tag. For example,
`(let (:doc "Keep the owner in this scope.") buffer (array.alloc u8 (i32 3)))`
binds `buffer` in the function body. In function-local `let` and `let-mut`
without metadata, the binding name follows the tag. Metadata entries use
colon-prefixed tags such as `(:doc "text")`, `(:expect ...)`, and
`(:pragma (cfg bootstrap) (inline always))`.
An entry before a declaration name or after an ordinary field is a source
error. The retired `with-meta`, `meta`, and `;;` spellings remain invalid. Quoted text can contain semicolons.

An `:expect` entry can group test and issue links on the form it describes.

```lisp
(:expect
	(tests "stage-2/lexer_stream.wove" "stage-2/lexer_errors.tsv")
	(issues "gh:wove-lang/wove:53"))
```

Test paths are relative to `tests/`. Each group takes its strings as operands.
Either group may be omitted. GitHub issues can use `gh:owner/repo:number` or a full URL.

A single backtick opens or closes a raw string. Inside the string, a doubled
backtick produces one backtick. Other characters and indentation are kept
literally, including backslashes, quotes, and semicolons. Physical CRLF and
lone CR become LF in the value. Use raw strings anywhere a double-quoted
string is accepted. A backtick ends a bare atom. An unterminated string
reports its opening line.

Stage 1 selects module roots, declarations, and `(decls ...)` groups before
name resolution. It always enables `bootstrap`. All `cfg` predicates on a
form must pass, and a `(test)` marker in any `:pragma` entry excludes the
form. The seed skips other directives, including `inline`, and leaves other
metadata payloads for later stages.

A module can use `(:pragma (cfg (not bootstrap)))` to guard the entire file.
An excluded provider exposes no declarations and its nested imports are skipped.
An empty `(only)` can load that provider, while a named selection reports the
unavailable name at its source line. The entry module must be active.

Use `(use wove-file "./source/scan.wove" (only scan_token (as source_kind
lexer_kind)))` to select functions from a local file. The path resolves from
the importing file. A bare name keeps its spelling, and `(as original local)`
binds a new name to the original declaration and signature. `(only)` loads the
file and binds no names.

Use `(type SourceNode)` or `(type (as SourceNode Node))` inside `only` to
select a record type. A provider can have a function and a record with the
same spelling. Function and type selectors bind their own namespaces.
A type alias preserves the record's defining identity in signatures,
constructors, arrays, and slices. Each file can select its provider's local
declarations, and each function resolves helpers and type names in its
defining file.

The seed skips a `bridge` and all its members. A `use` with a bare
provider tag other than `wove-file` is also skipped. Both forms must be
readable IR. Their metadata and payloads belong to later stages. They
contribute zero names and import edges in the seed. Bootstrap runtime
operations keep their fixed C bindings.

The seed evaluates selection pragmas on a `use wove-file` before opening its provider.
It loads each file once by identity, so multiple paths to the same module
share its declarations and use its first resolved path. An active missing
file or cycle is diagnosed at the importing `use` line. Errors inside an
imported file use that file's spelling and line. The entry module supplies
the executable's `main`.

Run `make stage-1` to translate and link the ownership example with Clang.
The resulting `build/stage-1/ownership.transfer` exits with status 6.
Run `make test` to check the transfer, its allocation lifetime, and the borrow
rejection cases. The runner reads fixture paths and expected results from
[the case manifest](../tests/stage-1/cases.tsv). Each row specifies a mode,
a source path relative to the manifest, and an exit status or diagnostic line.
Diagnostic cases can also name the file where the diagnostic should appear.
Trap cases match the runtime diagnostic at that line.
Use decimal numbers without leading zeros for expected results.
Tracked cases also run with the allocation probe.

The seed entry point is `python3 stage-1/wovec.py SOURCE`. The generated C
includes `wove_runtime.h` and links with `runtime/wove_runtime.c`. The byte
array implementation and slice layouts come from the [archived C runtime](https://github.com/wove-lang/.0.wove/tree/8ac13c06c422a2e2e9c66f59ec1a54f1fe106e52/runtime).

The [ownership expectation](https://github.com/wove-lang/wove/issues/1) stays
on the function in the IR. Stage 2 will provide the structured diagnostic
and related move location. The fixture IDs in the IR remain metadata.

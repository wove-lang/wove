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

Write `i8`, `u8`, `i32`, and `u32` literals as suffixed atoms such as
`-128i8`, `0xc2u8`, or `-0x80i8`. The same values can be written as
`(i8 -128)`, `(u8 0xC2)`, and `(i8 -0x80)`. Hexadecimal digits follow
`0x` or `0X`. A hexadecimal minus precedes the prefix and requires a
signed type. The seed checks the range of each literal and preserves its
exact type through bindings, parameters, calls, and returns. Use
`(i32.from-u8 (local byte))` to name both exact integer types, or
`(int.convert i32 (local byte))` to infer the source type from its operand.
Both spellings preserve representable values and trap at the source line when
the operand is outside the destination type's range.

`i32.add`, `i32.sub`, `u32.add`, and
`u32.sub` compute modulo 2^32. Both operands and the result have
the operation's named type.

`u32.and`, `u32.or`, and `u32.xor` combine two `u32` words.
`u32.not` complements one word. `u32.shl` and `u32.shr` take
a `u32` word and an exact `i32` count. Each form returns `u32`
and evaluates its operands from left to right. The left shift
keeps the low 32 bits, and the right shift fills with zeroes. A count
outside `[0, 32)` traps at the shift's source line.

Write boolean literals as `(bool true)` or `(bool false)`. Equality and
less-than forms such as `(u8.eq (local byte) (u8 40))` and `(i32.lt (i32 -1)
(i32 0))` take two operands of the named integer type and return `bool`.
For `u8`, `i32`, and `u32`, `not-eq` and `not-lt` negate the corresponding
comparison with the same operands evaluated once from left to right.

`(bool.and left right)` and `(bool.or left right)` evaluate the left
operand first. They evaluate the right operand once when the left operand is
true for `bool.and` or false for `bool.or`. The seed type-checks both operands
as `bool` in every case. `(bool.not value)` negates one boolean. All three
operations return `bool`.

Declare a named record with `(struct Token (fields (kind u32) (start i32)
(end i32)))`. Its fields can have type `bool`, `i8`, `u8`, `i32`, or `u32`.
A `struct.new` names the record type and supplies each field once, using its
declared type. Initializer expressions run in the order written. Use
`(struct.get (local token) start)` to read a field as its declared scalar type.
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
the buffer's element type in `(shared-array Token)` or `(mut-array Token)`.
`array.len` counts elements of a live owner or view. `array.load` copies
one element by value. `array.store` requires a mutable owner or exclusive
view and the exact declared element type. Loads and stores take an `i32`
element index and trap before an out-of-bounds access.

Use `borrow-shared` to forward either kind of slice to a helper.
Use `borrow-mut` to forward a mutable slice exclusively. A reborrow
uses the original storage, and its loan stays active through later
arguments and the call. Shared loans permit reads of the parent.
Exclusive loans suspend access to the parent until the helper returns.

Use `(borrow-shared (array.range (local source) start end))` to pass a
half-open range of a named byte owner or view. Use `borrow-mut` with a
writable owner or exclusive view to lend a writable range. The seed evaluates the
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
binding or parameter with the same name takes precedence. Each function
reads the bindings declared or selected in its defining file.

A string initializes a shared `u8` slice backed by one static read-only
table. Its length counts decoded UTF-8 bytes, with no added NUL. Use
`(array.load (local base16_alphabet) (local index))` with the ordinary
slice bounds checks. The same place works with `array.len` and
`borrow-shared`. Attempts to write a module binding fail at the source
operation.

A `module`, `fn`, or module-level `let` declaration starts with its tag
and bare name. Modules and module-level bindings put a contiguous metadata
prefix after the name. Functions collect metadata and tagged fields in any
order after the name. Each Wove function requires one `params`, one `returns`,
and one `body`, including explicit `(params)` for zero arguments.

```lisp
(fn succeed
  (returns i32)
  (:doc "Return success.")
  (params)
  (body (return 0i32)))
```

Application `main` takes a `(shared-array u8)` and a shared slice of a
record with exactly `(start i32)` and `(end i32)` in that order, and returns
`i32`. The seed emits the same `int main(int argc, char **argv)` wrapper
as Stage 2. It excludes the executable name and supplies two empty views
when the process has zero user arguments.

C bridge functions also start with a bare name and allow metadata anywhere
among their signature fields. Declaration groups carry that context through
their members. Opaque payloads can use the same form tags as ordinary data.

A C bridge starts with `(bridge c-header "<header.h>"`. Place its metadata
after the quoted header and before the first member.

Other forms keep metadata immediately after the tag. For example,
`(let (:doc "Keep the owner in this scope.") buffer (array.alloc u8 (i32 3)))`
binds `buffer` in the function body. In function-local `let` and `let-mut`
without metadata, the binding name follows the tag. Metadata entries use
colon-prefixed tags such as `(:doc "text")`, `(:expect ...)`, and
`(:pragma (cfg bootstrap) (inline always))`.
An entry before a fixed opening slot is a source error. Outside a function
declaration, an ordinary field ends the metadata prefix. The retired `with-meta`, `meta`, and `;;` spellings remain invalid. Quoted text can contain semicolons.

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

An unqualified selector can also import an immutable module binding.
For example, `(only alphabet (as alphabet digits))` binds both names to
the same read-only storage. Read either through `(local name)`. If the
provider defines a function and value with the same name, the unqualified
selector picks the function. Use `(value name)` or
`(value (as name alias))` to select the value explicitly.

Use `(type SourceNode)` or `(type (as SourceNode Node))` inside `only` to
select a record type. A provider can have a function and a record with the
same spelling. Function and type selectors bind their own namespaces.
A type alias preserves the record's defining identity in signatures,
constructors, arrays, and slices. Each file can select its provider's local
declarations, and each function resolves helpers and type names in its
defining file.

An active `(use c-header "<stdint.h>" (only ...))` contributes a system
include to the generated C. For a project header, use a path such as
`"./local.h"`. Explicit relative paths and project files found beside the
declaring Wove source are anchored there. Other quoted paths use Clang's
include search. Bare and explicitly quoted project spellings can refer
to the same path.

The generated C starts with its ABI header `wove_runtime.h` and emits each
additional header once in first-use order through the depth-first Wove
import walk. For an existing project file, the seed compares the resolved
containing directory and header filename, and preserves the first include
spelling for Clang. C selections and bridge bodies remain opaque to the seed.
A C-header use contributes no Wove names. Bootstrap runtime operations
keep their fixed C bindings.

The seed skips other foreign providers and bridge declarations before
interpreting their selection metadata. These forms still use structurally
readable IR, including the provider and quoted header of a C bridge.
The seed evaluates bootstrap selection before opening a Wove provider or
collecting a C-header operand.
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

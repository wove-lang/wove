<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# C standard library bridge corpus

The fourteen Wove files mirror the archived directory at `8ac13c06`. They are
standalone source for the bridge design. Each uses the current `c-header`
provider. Native fixtures select bundled members from `assert.h.wove`, `stdio.wove`, `math.wove`, and the Linux and macOS `Stat` views. [Archived directory](https://github.com/wove-lang/.0.wove/tree/8ac13c06c422a2e2e9c66f59ec1a54f1fe106e52/stage-2/c-stdlib)

The bundled `sys_file.wove` provider exposes `flock` and `LOCK_EX` for
serializing shared-library publication. The `c_export` pragma applies to
Wove definitions with bodies. Checked pointer/count export adapters lend
caller storage to those definitions. Bundled C imports use their own
bridge contracts.

## Source forms

- Each bridge starts with `bridge c-header` and the quoted original header,
  including its angle brackets. The bridge metadata follows the header. A
  bodyless `fn` records the Wove
  parameter and return types. Its optional `(c-name "symbol")` records the
  native spelling when it differs. An omitted archive result becomes
  `(returns void)`. Fixed C operands inside `params` accept i32 header
  constants, checked signed i32 literals, and a literal null with an
  explicit native pointer type.
- `(c-type "native spelling")` preserves the header type on an alias, record,
  or enum. `shared-ref` and `mut-ref` borrow native scalar, record, or
  resource storage. `shared-array` and `mut-array` represent borrowed spans.
  An `optional` result containing an `owned-array` preserves null and
  successful ownership separately. `fn-type` records a callback signature.
- Native `const`, `let`, and `let-mut` distinguish header constants,
  read-only values, and writable lvalues. A `cfg` directive stays on its
  declaration or `decls` group, preserving the archive's selection order.
- Each archived active `c_*` annotation is a directive under its
  declaration's `:pragma`. Named arguments use forms such as
  `(drop fclose)` and `(field d_name)` in their original argument order.
  `disabled` has a documentation entry and no operands. Its text retains
  the original comment delimiters, rationale, and declaration sketches.

The current parser and renderer can read these forms structurally. The seed
skips bridge members. Stage 2 resolves selected native integer aliases and
record views and lowers checked record fields and call-scoped borrows.
Further activation requires the declaration, type, selection, and lowering
work below. [Seed bridge behavior](../LANGUAGE-SUBSET.md#imports-and-c-headers)

## Bundled `<sys/wait.h>` coverage

`IdType` keeps the SDK's `idtype_t` spelling. `P_ALL`, `P_PID`, and
`P_PGID` are native values of that type. `waitid` lends a complete
`siginfo_t` for one call and exposes the six child-status fields,
including the glibc child member aliases. The installed fixture
observes an exited child by PID and process group with `WNOWAIT`, reaps
it with `P_ALL`, and checks `ECHILD` after collection.

Darwin exposes `waitid` outside the `_ANSI_SOURCE` profile. glibc
exposes it with POSIX.1-2008, extended X/Open, and the audited default
and GNU extension profiles. The driver checks the selected function,
complete record, and optional option macros against the header.
Select `Id`, `IdType`, and the typed selector constants in profiles
that supply their C declarations. Conditional scalar typedef
selection is tracked separately. [Optional scalar typedefs #638](https://github.com/wove-lang/wove/issues/638)

The audit covered both Clangs, both SDK architectures, C11, C17, and
C23. `wait`, `waitpid`, `WNOHANG`, `WUNTRACED`, and the ordinary
status macros remain available across the audited profiles. Older
glibc profiles also omit the function-like macro `WIFCONTINUED`.
`WCOREDUMP`, `WCORE_DUMPED`, `Usage`, `wait3`, and `wait4` remain
in the source's disabled group and require their own bridge coverage.

## Active archive declarations

| Header and declarations | Form or contract | Compiler behavior needed |
| --- | --- | --- |
| All fourteen headers and their active members | `(bridge c-header "<header>" …)`, bodyless `fn`, and `(c-name "symbol")` | Resolve a selected member from the declared header. Bind its Wove name and exact C name, discover macro or symbol availability in the selected target's header environment, check the native signature, and emit the right include, call, and link dependencies. |
| `dirent.h` aliases for stream positions, GNU byte counts and offsets, `fcntl.h` typedefs, `math.h` floating and integer aliases, `signal.h` signal types, `spawn.h` attributes and actions, `stdio.h` file and position types, `sys/stat.h`, `sys/wait.h`, `time.h`, and `unistd.h` aliases | `(type Name (c-type "…"))` | Integer aliases keep the selected native spelling and nominal identity in parameters, results, and locals. Wider native value classes, checked casts, and aliases as buffer extents require additional lowering. |
| `fcntl.h` `Lock`, `spawn.h` `Scheduling`, `sys/stat.h` `Timespec` and both `Stat` views, `sys/wait.h` `SignalInfo`, and `time.h` `Stamp` | `struct` with `c-type` and `fields` | Allocate, borrow, and read the header's complete native object. The Linux and macOS `Stat` fixtures cover target-specific timestamps and native field types. The installed `waitid` test checks the native child-status view in `siginfo_t` on Darwin and glibc. |
| `sys/wait.h` `IdType` | `type` with `c-type` and typed `const` declarations | The native `idtype_t` carries the selected header values for `P_ALL`, `P_PID`, and `P_PGID`. |
| `time.h` `Clock` | `enum` with `c-type` and `members` | Bind the C enum or typedef and its selected header members with their native values and argument representation. |
| `errno.h`, `fcntl.h`, `math.h`, `signal.h`, `spawn.h`, `stdio.h`, `sys/stat.h`, `sys/wait.h`, and `unistd.h` constant declarations | `const` with an explicit type | Select guarded integer constants with their declared Wove types and check the C value and constant expression. Wider Wove widths remain needed for some `UTIME_NOW` and `UTIME_OMIT` alternatives. |
| `errno.h` `errno`, `math.h` `signgam` and floating values, `stdio.h` standard streams, `unistd.h` `optind`, `opterr`, `optopt`, and `environ` | Typed `let` and `let-mut` | Read scalar lvalues and borrow standard streams live. Writable scalar bindings such as `errno` retain the original C lvalue. Read-only expression macros and copied snapshots have separate contracts. |
| `math.h` native floating aliases and Darwin half-width family | `float_t`, `double_t`, `long double`, and target-selected `f16` | Preserve the native floating representation in named aliases and out pointees. The Darwin half family still needs its feature and deployment contract. |
| `dirent.h` raw reads and `fdclosedir`, guarded members in `errno.h`, `fcntl.h`, `math.h`, `signal.h`, `spawn.h`, `stdlib.h`, `sys/stat.h`, and `unistd.h` | `(cfg (all …))`, `any`, `os`, `arch`, `os_min`, feature names | Source and scalar bridge selection accept a supplied OS, architecture, and deployment version. Remaining work includes host target discovery, feature selection, and additional member shapes. |
| `fcntl.h` `fcntl_*` | `(const command i32 F_GETFD)` and `c-name` | Stage 2 supplies checked i32 header constants in the native fixed prefix. The Wove caller supplies only the ordinary operands. |
| Native pointer parameters across the corpus, including `math.h` `frexp`, `fcntl.h` lock operations, and `time.h` `clock_gettime` | `shared-ref`, `mut-ref`, typed and byte slices | Lower declared native pointees and maintain borrow lifetimes through the call. Allow writable out parameters only through the applicable mutable views. |

### Bridge directives

The corpus carries 537 active `c_*` directives and 24 `cfg` directives.
The two commented `#[c_macro]` lines in `sys_wait.wove` remain in their
verbatim disabled text. Counts below cover active declarations.

| Directive and count | Header and affected declarations | Compiler behavior needed |
| --- | --- | --- |
| `c_array` · 8 | `dirent.h` `getdirentries{,64}`, `stdio.h` `fgets`, `sys/stat.h` `futimens` and `utimensat`, `unistd.h` `getgroups`, `pipe`, and `pipe2` | The scalar bridge checks literal and named element counts for shared and mutable slices of built-in and native integers. The native fixture exercises `pipe`. Native record element slices and composition with `c_nullable` still need support. |
| `c_bytes` · 8 | `dirent.h` `getdents64`, `stdio.h` `fread`, `fwrite`, and `snprintf`, `unistd.h` `read`, `write`, `pread`, and `pwrite` | The scalar bridge checks byte counts and products for shared and mutable slices, including native integer factors. The native fixtures exercise `fread`, `fwrite`, and bounded `snprintf`, including zero capacity and an oversized count. Guarded declarations also need target selection. |
| `c_callback` · 1 | `stdlib.h` `atexit` | Marshal `(fn-type (params) (returns void))` to a C-callable function pointer. The registered callable must remain valid through ordinary process exit. |
| `c_cstr` · 54 | Text inputs in `dirent.h`, `fcntl.h`, `math.h`, `signal.h`, `spawn.h`, `stdio.h`, `stdlib.h`, `sys/stat.h`, and `unistd.h` | Shared and mutable byte slices have checked, call-scoped conversion. Mutable views get full-length copy-back, and borrowed results are copied while that storage is live. |
| `c_cstr_raw` · 3 | `stdio.h` `fprintf`, `printf`, and `snprintf` | Fixed format arguments use raw, call-scoped conversion that preserves embedded NUL bytes. |
| `c_cstrs` · 6 | `spawn.h` `posix_spawn` and `posix_spawnp`, `unistd.h` `execv`, `execve`, `execvp`, and `fexecve` | The scalar bridge copies shared owned-byte rows to a terminated native table. The bundled `posix_spawnp` fixture verifies argument boundaries, empty rows, and environment transfer. |
| `c_init` · 2 | `spawn.h` `posix_spawn_file_actions_init` and `posix_spawnattr_init` | A mutable loan of inactive caller-owned storage activates destruction on a zero i32 status. The bundled spawn file-action fixture exercises initialization, use, and cleanup. |
| `c_macro` · 124 | `assert.h` `assert`, `math.h` macro functions and read-only macro values, `sys/stat.h` `S_IS*` and `S_TYPEIS*`, `sys/wait.h` wait status macros | Stage 2 captures arguments once in source order before expanding value macros such as `S_ISREG` and `S_TYPEISMQ` or statement macros such as `assert`. Additional object-like macro types still need lowering. |
| `c_null` · 1 | `stdlib.h` `realpath` | Stage 2 supports the allocating `realpath` slot. It requires literal `null` and passes C `NULL`. |
| `c_nullable` · 12 | `signal.h` `sigprocmask`, `pthread_sigmask`, `psignal`, `spawn.h` `posix_spawn{,p}`, `stdio.h` `freopen`, `fflush`, `perror`, `sys/stat.h` `futimens`, `utimensat`, `sys/wait.h` `wait`, `waitpid` | Named mutable scalar references accept either an exclusive loan or explicit null. Native fixtures exercise both forms of `waitpid`. Nullable record and handle references, strings, and bounded arrays still need their own lowering. A Wove fallible exact slice can lend a present shared view after unwrapping. Passing the option itself requires a native nullable descriptor contract. |
| `c_optional` · 289 | `math.h` optional functions, constants, and globals. The `fromfp`, `ufromfp`, `fromfpx`, and `ufromfpx` families use `__STDC_VERSION_MATH_H__`. The `totalorder` and `totalordermag` families use `pointer_parameters`. | Bare optional function markers probe for the selected native name and report an unavailable name at the import. Qualified markers need feature-macro or exact prototype queries. Optional constants and global values need their own availability checks. |
| `c_owned` · 4 | `dirent.h` `Directory`, `stdio.h` `File`, `spawn.h` `Actions` and `Attributes` | The `c-header` provider supports pointer results as local owners with null preservation and lexical destruction. A `shared-ref` or `mut-ref` parameter can lend a present owned pointer for a call, with exclusive access for the mutable loan. A bare owner parameter consumes an explicit move. Storage owners allocate the exact C representation and track activation separately from the allocation. Nullable storage borrows and integer handles need further lowering. |
| `c_snapshot` · 9 | `dirent.h` `readdir{,64}`, `stdio.h` `fgets`, `stdlib.h` `getenv`, `mkdtemp`, `realpath`, `string.h` `strerror`, `unistd.h` `mkdtemp` and `environ` | Stage 2 copies borrowed and allocating `cstr` function results and `cstrs` globals such as `environ`. Stage 2 also copies fixed inline `char` fields within their declared extent. The bundled `readdir` snapshots the terminated name returned by the native API, including extended entries. The archived `readdir64` extension needs named feature selection. |
| `c_variadic` · 16 | `fcntl.h` `open`, `openat`, and `fcntl_*`, `stdio.h` `fprintf`, `printf`, and `snprintf` | Named and open tails verify the fixed prefix and result against the header and use native default promotions. The fixed-command `fcntl_getfd`, `fcntl_getfl`, and `fcntl_getown` queries use an empty tail. Fixtures cover those queries, typed `fcntl` updates and opening modes, and the three open stdio bindings. |

## Native owner composition

Ordinary Wove helpers can return the selected `File` and `Directory`
pointer owners from the existing bundled stdio and directory providers.
A returned handle can already be missing. A present handle is owned and
released by its eventual caller. The SQLite fixture also transfers a
statement acquired through a native output argument. The Stream Merge
fixture owns both bundled pointer types in nested structs stored in an
owned array. A tracked fixture checks native pointer-out initialization of
a field, null checks, direct consuming calls, partial moves, and cleanup
of both removed and remaining rows.
The tested calls use the existing bundled declarations. Caller-owned
storage contracts such as spawn `Actions` and `Attributes` retain their
local allocation and initialization rules.

## Borrowed native record snapshots

A project bridge for Darwin `<grp.h>` exercises `getgrnam`, `getgrgid`,
`getgruuid` and `getgrent` with a shared record mapping. Wove copies the
thread-local return into an owning result before another lookup or
enumeration reset can affect the values. The mapping covers `gid_t`,
the name, the nullable password, and every member string. The libc
provider inventory is tracked in #514. glibc caller-buffer group lookup
is tracked in #637.

## Fixed native operands

The eleven existing `fcntl.h` fixed commands use header constants. Both
empty-tail queries and named variadic updates are covered by the native
fixtures. No other checked-in provider uses a fixed operand with a signed
literal or typed null. The standalone SQLite fixture passes `-1` and
`const char **` null to `sqlite3_prepare_v2` and tests its statement
pointer-out contract. A project bridge for `realpath` also exercises a fixed
null with a copied C string result. `strtold` still needs a native `long double` result
and a lifetime contract for an end pointer into the input string.


## Bundled assert coverage

Both compiler inventories load `assert.h.wove`. The installed fixture
imports `assert` through `use c-header` from a separate project. It builds and
links in C11, C17, and C23. The focused tests check an enabled success, an
enabled failure that raises `SIGABRT`, and a disabled assertion whose Wove
condition still runs once. [Compiler prerequisite](https://github.com/wove-lang/wove/pull/512)

Apple Clang 21 reported the following members when preprocessing the macOS
27.2 SDK for arm64 and x86-64 and the Debian 13 `libc6-dev`
`2.41-12+deb13u5` headers for aarch64 and x86-64 Linux. All four targets were
checked in C11, C17, and C23, with and without `NDEBUG`. The Wove-emitted
assertion compiled for all four targets. The macOS targets also linked.
[Debian arm64 headers](https://packages.debian.org/trixie/arm64/libc6-dev/download) · [Debian amd64 headers](https://packages.debian.org/trixie/amd64/libc6-dev/download)

| Member | Availability and disposition |
| --- | --- |
| `assert` | Available in all tested target and C mode combinations. The bundled statement macro takes a Wove `bool` and uses the native `NDEBUG` policy after evaluating that argument. |
| `static_assert` | C11 and C17 provide a declaration macro for `_Static_assert`. C23 supports `static_assert` as a keyword without the header. Both tested header sets also publish the compatibility macro in C23. The provider keeps it disabled because Wove needs declaration-time constant expressions and diagnostic text. |
| `__STDC_VERSION_ASSERT_H__` | Absent in all tested combinations. The C23 header-version macro is listed by LLVM libc. The provider keeps it disabled until it can verify header constant availability on the selected SDK and C mode. |

[LLVM libc checklist](https://libc.llvm.org/headers/assert.html) · [C11 draft §7.2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf) · [C23 draft §§6.4.1, 6.7.11, and 7.2](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3096.pdf)

## Bundled math coverage

Both compiler inventories load the checked-in `<math.h>` provider. The
installed fixture exercises these forms through the system header and
library with Clang builtins disabled.

| Contract | Exercised declarations |
| --- | --- |
| Built-in `f32` and `f64` parameters and results | `cos`, `sin`, `sqrt`, `cosf`, `hypot`, and `fma` |
| Writable scalar references | `frexp` and `remquo` with `i32` outputs, and `modff` with an `f32` output |
| Native integer and fixed C-string contracts | `llround` returns a native `long long`, and `nan` converts a shared byte view |
| Macro calls and header values | `fpclassify`, `isfinite`, `FP_NAN`, `HUGE_VAL`, and `M_PI_2` |
| Target selection | The Linux/macOS POSIX group exposes `j0`. Graph fixtures reject Darwin `__sinpi` under Linux and Linux `drem` under macOS at the requesting name. |

A macOS header audit built and linked wrappers for 151 existing function
declarations. The remaining ABI gaps include these contracts:

- `FloatEval`, `DoubleEval`, and `LongDouble` require native floating
  aliases for `float_t`, `double_t`, and `long double`. This includes
  the long-double function families, macros such as `HUGE_VALL`, and
  writable outputs such as `modfl`. The underlying C type must follow
  the selected target and header.
- `totalorder*`, `totalordermag*`, and `getpayload*` require shared
  scalar references. The qualified `pointer_parameters` marker and
  the `__STDC_VERSION_MATH_H__` markers also need their respective
  prototype and feature-macro checks.
- The Darwin `__sincos*` functions use native `void` results and writable
  outputs. Their exact signatures and availability need activation coverage.
  Optional classification macros such as
  `iscanonical` and `issignaling` are absent from the tested macOS SDK,
  where selecting them reports an unavailable import.
- The archived Darwin half declarations need the `math_darwin_half`
  selection and minimum-deployment check before probing individual
  optional symbols. Core `f16` already has a target admission check.

## Archived disabled material

These portions were already comments in the archive. With the exception of
the exact prototypes shown there, the comments are an inventory of
interfaces and design constraints.

| Header and archived declaration or family | Compiler behavior needed |
| --- | --- |
| `assert.h` `static_assert` | See [Bundled assert coverage](#bundled-assert-coverage). |
| `dirent.h` `readdir_r`, `readdir64_r`, scan and comparator families, Darwin Blocks variants and Darwin `getdirentries` | Represent runtime-sized native storage and nullable aliases, owned result vectors, native-record callbacks and indirect borrows, and C Blocks. Gate the raw Darwin interface on its inode ABI. |
| `fcntl.h` optional flags and Darwin or Linux native records and commands listed in the disabled note | Probe individual declarations in the selected feature namespace and describe platform-specific native records and typed command tails. |
| `math.h` `FP_CONTRACT`, `FENV_ROUND`, decimal and `_FloatN` or `_FloatNx` families, Darwin compatibility names and `struct exception` | Support C translation directives and the relevant native floating identities, and provide the native record with its raw character pointer. `complex.h` has its own ABI. |
| `signal.h` `SIG_DFL`, `SIG_IGN`, `SIG_ERR`, `signal`, `bsd_signal`, and `sigset` | Represent native function pointer values and sentinel constants with the correct C calling convention and lifetime. |
| `signal.h` `Action`, `sigaction`, and its `SA_*` flags | Bind the native callback-bearing union and platform-typed flag members. |
| `signal.h` `SS_ONSTACK`, `SS_DISABLE`, `Stack`, `sigaltstack`, `MINSIGSTKSZ`, and `SIGSTKSZ` | Track stack storage retained by the thread until disabled or exited. Read runtime-sized header values where required. |
| `signal.h` `Value`, `Event`, `sigqueue`, `sigwaitinfo`, `sigtimedwait`, and the `SIGEV_*` and `SIGRT*` values | Model union payloads, richer siginfo views, queued or retained callback notifications, and runtime signal bounds. |
| `signal.h` `pthread_kill` and the deprecated XSI/BSD group | Use an explicit native pthread handle contract and select supported legacy declarations and callback records by target. |
| `spawn.h` chdir, inherited descriptor, scheduling and Darwin architecture or port extensions in the disabled note | Check individual SDK and libc availability. Model native output arrays and state retained by exception-port configuration. |
| `stdio.h` and `stdlib.h` `NULL` | Bind a pointer-valued header constant with an explicit nullable native pointer contract. |
| `stdio.h` `tmpnam` | Check caller storage against the actual header's `L_tmpnam` extent and track any result alias. |
| `stdio.h` `setbuf`, `setvbuf` | Track buffer storage retained until the stream closes. |
| `stdio.h` `sprintf`, `fscanf`, `scanf`, and `sscanf` | Bound the unbounded output of `sprintf` and model format-directed writable tail arguments for the scanning functions. |
| `stdio.h` `vfprintf`, `vprintf`, `vsprintf`, `vsnprintf`, `vfscanf`, `vscanf`, and `vsscanf` | Represent native `va_list` storage and `va_copy` or `va_end` lifetime rules alongside the format contracts. |
| `stdlib.h` `MB_CUR_MAX` and `strtold` | Read a locale-dependent native `size_t` expression at runtime. For `strtold`, preserve native `long double` and track an optional output pointer aliasing the input string. |
| `sys/stat.h` timestamp aliases and host extensions | Represent overlapping timestamp aliases and add separate availability, ownership, and record contracts for host extensions. |
| `sys/wait.h` `WCOREDUMP`, `WCORE_DUMPED`, `Usage`, `wait3`, and `wait4` | Select the optional macros and functions from the target header and check native `struct rusage` output storage. |
| `unistd.h` `execl`, `execle`, and `execlp` | Marshal each variable-length text operand to a call-scoped C string and supply a typed terminating null. `execle` also needs the environment vector after that null. The open scalar tail supplies neither pointer contract. |
| `unistd.h` text and returned-alias families, `optarg`, and `getopt` | Check native character storage, termination and returned aliases. Track the retained or reordered argv vector and the lifetime of its borrowed text. Wove `(returns T (from parameter))` applies to ordinary Wove functions. Native returned aliases also need a bridge lifetime contract. |
| `unistd.h` selector catalogs, `vfork`, and deprecated or removed APIs listed in the final note | Select host and header feature catalogs. Model the shared-address-space control flow of `vfork` before enabling it or any separately supported extension. |

Each corpus file can be checked and round-tripped with the existing Stage 2
source probe. This checks Wove structure and metadata layout independently
of bridge activation. [Stage 2 source probe](../README.md)

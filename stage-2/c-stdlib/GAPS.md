<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# C standard library bridge corpus

The fourteen Wove files mirror the archived directory at `8ac13c06`. They are
standalone source for the bridge design. Each uses the current `c-header`
provider. Native fixtures select members from `stdio.wove` and the Linux and macOS `Stat` views. [Archived directory](https://github.com/wove-lang/.0.wove/tree/8ac13c06c422a2e2e9c66f59ec1a54f1fe106e52/stage-2/c-stdlib)

## Source forms

- Each bridge starts with `bridge c-header` and the quoted original header,
  including its angle brackets. The bridge metadata follows the header. A
  bodyless `fn` records the Wove
  parameter and return types. Its optional `(c-name "symbol")` records the
  native spelling when it differs. An omitted archive result becomes
  `(returns void)`. A fixed C operand uses `(const command i32 F_GETFD)`
  inside `params`.
- `(c-type "native spelling")` preserves the header type on an alias, record,
  or enum. `shared-ref` and `mut-ref` borrow native scalar, record, or
  resource storage. `shared-slice` and `mut-slice` represent borrowed spans.
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
work below. [Seed bridge behavior](../../stage-1/README.md)

## Active archive declarations

| Header and declarations | Form or contract | Compiler behavior needed |
| --- | --- | --- |
| All fourteen headers and their active members | `(bridge c-header "<header>" …)`, bodyless `fn`, and `(c-name "symbol")` | Resolve a selected member from the declared header. Bind its Wove name and exact C name, discover macro or symbol availability in the selected target's header environment, check the native signature, and emit the right include, call, and link dependencies. |
| `dirent.h` aliases for stream positions, GNU byte counts and offsets, `fcntl.h` typedefs, `math.h` floating and integer aliases, `signal.h` signal types, `spawn.h` attributes and actions, `stdio.h` file and position types, `sys/stat.h`, `sys/wait.h`, `time.h`, and `unistd.h` aliases | `(type Name (c-type "…"))` | Integer aliases keep the selected native spelling and nominal identity in parameters, results, and locals. Wider native value classes, checked casts, and aliases as buffer extents require additional lowering. |
| `fcntl.h` `Lock`, `spawn.h` `Scheduling`, `sys/stat.h` `Timespec` and both `Stat` views, `sys/wait.h` `SignalInfo`, and `time.h` `Stamp` | `struct` with `c-type` and `fields` | Allocate, borrow, and read the header's complete native object. The Linux and macOS `Stat` fixtures cover target-specific timestamps and native field types. Specialized union and alias views such as the child status in `siginfo_t` need separate validation. |
| `sys/wait.h` `IdType` and `time.h` `Clock` | `enum` with `c-type` and `members` | Bind the C enum or typedef and its selected header members with their native values and argument representation. |
| `errno.h`, `fcntl.h`, `math.h`, `signal.h`, `spawn.h`, `stdio.h`, `sys/stat.h`, `sys/wait.h`, and `unistd.h` constant declarations | `const` with an explicit type | Select guarded integer constants with their declared Wove types and check the C value and constant expression. Wider Wove widths remain needed for some `UTIME_NOW` and `UTIME_OMIT` alternatives. |
| `errno.h` `errno`, `math.h` `signgam` and floating values, `stdio.h` standard streams, `unistd.h` `optind`, `opterr`, `optopt`, and `environ` | Typed `let` and `let-mut` | Read scalar lvalues and borrow standard streams live. Writable scalar bindings such as `errno` retain the original C lvalue. Read-only expression macros and copied snapshots have separate contracts. |
| `math.h` native math families, including `__fabsf16` and the Darwin half-width guard, and `sys/stat.h` Linux timestamp controls | `f16`, `f32`, `f64`, `i64`, and native `long double` | Check and lower the declared scalar types in arguments, results, constants, and out pointees. Preserve target floating representation and narrowing behavior. |
| `dirent.h` raw reads and `fdclosedir`, guarded members in `errno.h`, `fcntl.h`, `math.h`, `signal.h`, `spawn.h`, `stdlib.h`, `sys/stat.h`, and `unistd.h` | `(cfg (all …))`, `any`, `os`, `arch`, `os_min`, feature names | Source and scalar bridge selection accept a supplied OS, architecture, and deployment version. Remaining work includes host target discovery, feature selection, and additional member shapes. |
| `fcntl.h` `fcntl_*` | `(const command i32 F_GETFD)` and `c-name` | Supply the header constant automatically in its original C position. The Wove caller supplies only the ordinary operands. |
| Native pointer parameters across the corpus, including `math.h` `frexp`, `fcntl.h` lock operations, and `time.h` `clock_gettime` | `shared-ref`, `mut-ref`, typed and byte slices | Lower declared native pointees and maintain borrow lifetimes through the call. Allow writable out parameters only through the applicable mutable views. |

### Archived bridge directives

The port carries 532 active `c_*` directives and 23 `cfg` directives.
The two commented `#[c_macro]` lines in `sys_wait.wove` remain in their
verbatim disabled text. Counts below cover active declarations.

| Directive and count | Header and affected declarations | Compiler behavior needed |
| --- | --- | --- |
| `c_array` · 7 | `dirent.h` `getdirentries{,64}`, `stdio.h` `fgets`, `sys/stat.h` `futimens` and `utimensat`, `unistd.h` `getgroups` and `pipe` | The scalar bridge checks literal and named element counts for shared and mutable slices of built-in and native integers. The native fixture exercises `pipe`. Native record element slices and composition with `c_nullable` still need support. |
| `c_bytes` · 7 | `dirent.h` `getdents64`, `stdio.h` `fread` and `fwrite`, `unistd.h` `read`, `write`, `pread`, and `pwrite` | The scalar bridge checks byte counts and products for shared and mutable slices, including native integer factors. The native fixtures exercise `fread` and `fwrite`. Guarded declarations also need target selection. |
| `c_callback` · 1 | `stdlib.h` `atexit` | Marshal `(fn-type (params) (returns void))` to a C-callable function pointer. The registered callable must remain valid through ordinary process exit. |
| `c_cstr` · 54 | Text inputs in `dirent.h`, `fcntl.h`, `math.h`, `signal.h`, `spawn.h`, `stdio.h`, `stdlib.h`, `sys/stat.h`, and `unistd.h` | Shared and mutable byte slices have checked, call-scoped conversion. Mutable views get full-length copy-back, and borrowed results are copied while that storage is live. |
| `c_cstr_raw` · 2 | `stdio.h` `fprintf`, `printf` | Byte slices have raw, call-scoped conversion that preserves embedded NUL bytes. Use the conversion for fixed parameters in calls with a native variadic tail. |
| `c_cstrs` · 6 | `spawn.h` `posix_spawn` and `posix_spawnp`, `unistd.h` `execv`, `execve`, `execvp`, and `fexecve` | The scalar bridge copies shared owned-byte rows to a terminated native table. The bundled `posix_spawnp` fixture verifies argument boundaries, empty rows, and environment transfer. |
| `c_init` · 2 | `spawn.h` `posix_spawn_file_actions_init` and `posix_spawnattr_init` | A mutable loan of inactive caller-owned storage activates destruction on a zero i32 status. The bundled spawn file-action fixture exercises initialization, use, and cleanup. |
| `c_macro` · 123 | `assert.h` `assert`, `math.h` macro functions and read-only macro values, `sys/stat.h` `S_IS*` and `S_TYPEIS*`, `sys/wait.h` wait status macros | Stage 2 captures arguments once before expanding value-returning macros such as `S_ISREG` and `S_TYPEISMQ`. Existing bridge types and argument contracts apply. Remaining work includes void results and additional object-like macro types. |
| `c_null` · 1 | `stdlib.h` `realpath` | Stage 2 supports the allocating `realpath` slot. It requires literal `null` and passes C `NULL`. |
| `c_nullable` · 12 | `signal.h` `sigprocmask`, `pthread_sigmask`, `psignal`, `spawn.h` `posix_spawn{,p}`, `stdio.h` `freopen`, `fflush`, `perror`, `sys/stat.h` `futimens`, `utimensat`, `sys/wait.h` `wait`, `waitpid` | Named mutable scalar references accept either an exclusive loan or explicit null. Native fixtures exercise both forms of `waitpid`. Nullable record and handle references, strings, and bounded arrays still need their own lowering. |
| `c_optional` · 289 | `math.h` optional functions, constants, and globals. The `fromfp`, `ufromfp`, `fromfpx`, and `ufromfpx` families use `__STDC_VERSION_MATH_H__`. The `totalorder` and `totalordermag` families use `pointer_parameters`. | Select header members by name and declaration kind. Apply the requested feature macro or native pointer signature probe and diagnose a requested unavailable member during binding. |
| `c_owned` · 4 | `dirent.h` `Directory`, `stdio.h` `File`, `spawn.h` `Actions` and `Attributes` | The `c-header` provider supports pointer results as local owners with null preservation and lexical destruction. A `shared-ref` parameter can borrow a present owned pointer for a call. A bare owner parameter consumes an explicit move. Storage owners allocate the exact C representation and track activation separately from the allocation. Nullable storage borrows and integer handles need further lowering. |
| `c_snapshot` · 9 | `dirent.h` `readdir{,64}`, `stdio.h` `fgets`, `stdlib.h` `getenv`, `mkdtemp`, `realpath`, `string.h` `strerror`, `unistd.h` `mkdtemp` and `environ` | Stage 2 copies borrowed and allocating `cstr` function results and `cstrs` globals such as `environ`. Inline fields remain to be ported. |
| `c_variadic` · 15 | `fcntl.h` `open`, `openat`, and `fcntl_*`, `stdio.h` `fprintf` and `printf` | The named tail verifies the fixed prefix and result against the C prototype and uses target default promotions. The `open` and `openat` fixtures use the real flags and native `Mode`. Argument-free and open tails, and fixed header commands for `fcntl_*` remain. |

## Archived disabled material

These portions were already comments in the archive. With the exception of
the exact prototypes shown there, the comments are an inventory of
interfaces and design constraints.

| Header and archived declaration or family | Compiler behavior needed |
| --- | --- |
| `assert.h` `static_assert` | Support a declaration macro with a compile-time expression and diagnostic text. |
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
| `stdio.h` `sprintf`, `snprintf`, `fscanf`, `scanf`, and `sscanf` | Check format-directed output storage and writable arguments, including their extents and native types. |
| `stdio.h` `vfprintf`, `vprintf`, `vsprintf`, `vsnprintf`, `vfscanf`, `vscanf`, and `vsscanf` | Represent native `va_list` storage and `va_copy` or `va_end` lifetime rules alongside the format contracts. |
| `stdlib.h` `MB_CUR_MAX` and `strtold` | Read a locale-dependent native `size_t` expression at runtime. For `strtold`, preserve native `long double` and track an optional output pointer aliasing the input string. |
| `sys/stat.h` timestamp aliases and host extensions | Represent overlapping timestamp aliases and add separate availability, ownership, and record contracts for host extensions. |
| `sys/wait.h` `WCOREDUMP`, `WCORE_DUMPED`, `Usage`, `wait3`, and `wait4` | Select the optional macros and functions from the target header and check native `struct rusage` output storage. |
| `unistd.h` `execl`, `execle`, and `execlp` | Marshal variable-length typed text tails with a terminating native null. |
| `unistd.h` text and returned-alias families, `optarg`, and `getopt` | Check native character storage, termination and returned aliases. Track the retained or reordered argv vector and the lifetime of its borrowed text. |
| `unistd.h` selector catalogs, `vfork`, and deprecated or removed APIs listed in the final note | Select host and header feature catalogs. Model the shared-address-space control flow of `vfork` before enabling it or any separately supported extension. |

Each corpus file can be checked and round-tripped with the existing Stage 2
source probe. This checks Wove structure and metadata layout independently
of bridge activation. [Stage 2 source probe](../README.md)

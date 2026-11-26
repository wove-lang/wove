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
| `dirent.h` raw reads and `fdclosedir`, guarded members in `errno.h`, `fcntl.h`, `math.h`, `signal.h`, `spawn.h`, `stdlib.h`, `sys/stat.h`, and `unistd.h` | `(cfg (all …))`, `any`, `os`, `arch`, `os_min`, feature names | The native driver passes Clang's effective OS, architecture, and macOS deployment through source and scalar bridge selection. Feature selection and additional member shapes remain pending. |
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
| `c_snapshot` · 9 | `dirent.h` `readdir{,64}`, `stdio.h` `fgets`, `stdlib.h` `getenv`, `mkdtemp`, `realpath`, `string.h` `strerror`, `unistd.h` `mkdtemp` and `environ` | Stage 2 copies borrowed and allocating `cstr` function results and `cstrs` globals such as `environ`. Stage 2 also copies fixed inline `char` fields within their declared extent. The bundled `readdir` snapshots the terminated name returned by the native API, including extended entries. The Linux `readdir64` binding snapshots the name using the same runtime adapter. |
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

## Bundled ctype coverage

The installed compiler selects `ctype.h.wove` through `use c-header`.
The native fixtures exercise each callable declaration and each exported
classification mask. Target fixtures also reject the opposite platform's
extensions.

Apple Clang 21 inspected the macOS 27.2 SDK for arm64 and x86-64 with
`_DARWIN_C_SOURCE` and Debian 13 glibc 2.41 headers for aarch64 and x86-64
Linux with `_GNU_SOURCE`. The listed members and signatures were the same
in C11, C17, and C23 for each target.

| Members | Availability and disposition |
| --- | --- |
| `isalnum`, `isalpha`, `isblank`, `iscntrl`, `isdigit`, `isgraph`, `islower`, `isprint`, `ispunct`, `isspace`, `isupper`, `isxdigit`, `tolower`, `toupper` | Both header sets provide these as `int(int)`. The shared declarations cover the ISO C classifiers and conversions. |
| `isascii`, `toascii`, `_tolower`, `_toupper` | Both header sets expose these with the tested feature macros. The underscore-prefixed conversions use the argument-capturing C macro contract. |
| `digittoint`, `ishexnumber`, `isideogram`, `isnumber`, `isphonogram`, `isrune`, `isspecial`, `ct_rune_t`, `rune_t` | Darwin extensions. The two aliases retain their header spellings. The seven functions have `int(int)` prototypes. |
| `isctype` and `_ISupper`, `_ISlower`, `_ISalpha`, `_ISdigit`, `_ISxdigit`, `_ISspace`, `_ISprint`, `_ISgraph`, `_ISblank`, `_IScntrl`, `_ISpunct`, `_ISalnum` | glibc extensions with the tested GNU feature mode. The fixture checks each mask through `isctype`. |
| `locale_t`, `newlocale`, `LC_CTYPE_MASK` | The acquisition bridge exposes the same opaque pointer typedef on both targets. It passes a fixed null base to the native `newlocale` function and releases present handles with `freelocale`. The macOS destructor returns `int` and glibc declares `void`. [Pointer typedefs](https://github.com/wove-lang/wove/pull/518) |
| `isalnum_l`, `isalpha_l`, `isblank_l`, `iscntrl_l`, `isdigit_l`, `isgraph_l`, `islower_l`, `isprint_l`, `ispunct_l`, `isspace_l`, `isupper_l`, `isxdigit_l`, `tolower_l`, `toupper_l` | glibc declares them in `<ctype.h>`. macOS declares them in `<xlocale.h>` with `<ctype.h>` visible. Both use the same defining locale owner from `<locale.h>`. The two fixtures exercise all fourteen borrowed calls. [Cross-header type dependencies](https://github.com/wove-lang/wove/pull/520) |
| `digittoint_l`, `ishexnumber_l`, `isideogram_l`, `isnumber_l`, `isphonogram_l`, `isrune_l`, `isspecial_l` | Darwin publishes these additional `int(int, locale_t)` declarations in `<xlocale.h>`. The Darwin fixture exercises all seven. |
| `isascii_l`, `toascii_l` | GNU macros in glibc `<ctype.h>` under `_GNU_SOURCE`. The shared locale operand is checked and captured before the expansion. Its `c_macro_discard` marker gives the native comma operand a `void` cast and preserves strict warnings. The fixture verifies ASCII range and low-seven-bit results, including non-ASCII inputs. [Compiler prerequisite #522](https://github.com/wove-lang/wove/issues/522) |

The macOS scanner also saw `size_t`, `wchar_t`, and `wint_t` through system
includes. Their bridge aliases belong with the headers that define their
public C interfaces.

[LLVM libc checklist](https://libc.llvm.org/headers/ctype.html) · [Debian arm64 headers](https://packages.debian.org/trixie/arm64/libc6-dev/download) · [Debian amd64 headers](https://packages.debian.org/trixie/amd64/libc6-dev/download)

## Bundled dirent coverage

Both drivers load the existing provider from `dirent.h.wove`. The
installed-compiler fixtures cover ownership transfer from `fdopendir`,
descriptor access through `dirfd`, ownership consumption through
`closedir`, and a `telldir` position passed back to `seekdir`. They also
verify that `rewinddir` returns to the first entry and that `readdir`
snapshots remain valid after another call and after closing the stream.
The borrowed-entry fixture reads the native fields and copies the name
while the stream is borrowed. It compares native and large-file records
on glibc and checks Darwin's seek offset and name length.

Clang exposed the same selected members in C11, C17, and C23 on arm64 and
x86-64 with the macOS 27.2 SDK and Debian glibc 2.41. The audit used the
driver's Darwin and GNU feature definitions. The supported members and
remaining SDK differences are listed below.
[LLVM libc dirent checklist](https://libc.llvm.org/headers/dirent.html)

| Members | Coverage or outstanding contract |
| --- | --- |
| `DT_UNKNOWN`, `DT_FIFO`, `DT_CHR`, `DT_DIR`, `DT_BLK`, `DT_REG`, `DT_LNK`, `DT_SOCK`, `DT_WHT`, `IFTODT`, `DTTOIF` | Darwin and glibc expose these under the driver's feature flags. The fixture checks each type against its mode bits, including permission-bit masking. |
| `MAXNAMLEN`, `DIRBLKSIZ` | The fixture checks the name limit on both SDKs and the Darwin-only block size. |
| `DIR`, `opendir`, `fdopendir`, `closedir`, `dirfd`, `readdir`, `rewinddir`, `seekdir`, `telldir` | The provider owns `DIR *` and returns copied entry names. Installed fixtures exercise stream creation, descriptor transfer, navigation, and snapshot ownership. |
| `struct dirent`, `struct dirent64`, `ino_t`, `ino64_t`, `readdir_entry`, `readdir64_entry`, `entry_name`, `entry64_name` | The optional record borrows the stream exclusively, preserving EOF and error as a null result. Both records expose `d_ino`, `d_reclen`, and `d_type`. Darwin also exposes `d_seekoff` and `d_namlen`, while glibc exposes `d_off`. The explicit name accessors snapshot the terminated `d_name`, including bytes beyond the nominal glibc field extent. [Compiler prerequisite #531](https://github.com/wove-lang/wove/issues/531) |
| `readdir64`, `getdents64`, `getdirentries`, `getdirentries64` | The 64-bit Linux provider snapshots the wide name before the stream can reuse it. It checks byte or element counts and keeps the raw read results and native position outputs. The emitted fixture builds against glibc in all audited C modes. Darwin redirects its `getdirentries` declaration to an unavailable symbol under the current 64-bit inode ABI, as confirmed by a link with the macOS 27.2 SDK. [glibc directory reads](https://sourceware.org/glibc/manual/latest/html_node/Low_002dlevel-Directory-Access.html) |
| `fdclosedir` | macOS deployment 26.4 and later can consume the owned stream and transfer its open directory descriptor to the caller. The installed fixture adopts it into a second stream, reads an entry, and closes it. Selection uses Clang's effective deployment and queries the native declaration in the selected SDK. Direct imports are checked at macOS 26.3 on both architectures and on Linux, where the member is unavailable. [Compiler prerequisite #535](https://github.com/wove-lang/wove/issues/535) |
| `readdir_r`, `readdir64_r` | Deprecated interfaces require storage sized for the native name limit and return a nullable alias into caller storage. |
| `scandir`, `scandirat`, `fdscandir`, `scandir64`, `scandirat64`, `alphasort`, `alphasort64`, `versionsort`, `versionsort64` | The scanning calls need ownership for the allocated pointer vector and entries, plus borrowed native-record callbacks. The comparators need indirect borrowed entry pointers. |
| `scandir_b`, `scandirat_b`, `fdscandir_b` | The Darwin variants require a C Blocks callback ABI. |
| `posix_getdents`, `struct posix_dent`, `reclen_t`, `DT_MQ`, `DT_SEM`, `DT_SHM`, `DT_TMO` | The audited SDKs do not expose these POSIX.1-2024 members. |

## Bundled errno coverage

Both drivers load `errno.h.wove`. The installed-compiler fixture reads and
writes the `errno` lvalue and restores the original value. It selects and
evaluates every error code in the provider for its target.

Clang preprocessing with Wove's feature definitions exposed 89 shared
error codes, 20 additional Darwin codes, and 45 additional glibc codes.
Each platform had the same entries across arm64, x86-64, C11, C17, and C23
in the macOS 27.2 and Debian glibc 2.41 headers. The ISO C names are
`EDOM`, `EILSEQ`, and `ERANGE`. The remaining codes follow the audited
SDKs. [LLVM libc errno checklist](https://libc.llvm.org/headers/errno.html)

Both SDKs alias `EWOULDBLOCK` to `EAGAIN`. glibc also aliases
`EDEADLOCK` to `EDEADLK` and `ENOTSUP` to `EOPNOTSUPP`. The fixture
checks those relations and Darwin's distinct `ENOTSUP` and
`EOPNOTSUPP` values. Darwin's `__error` and glibc's
`__errno_location` are reserved implementation spellings called by the
header's `errno` macro.

The GNU `program_invocation_name` and `program_invocation_short_name`
globals are writable `char *` lvalues. They remain documented in the
provider pending a contract for reassigning native string pointers with
explicit lifetimes. [Compiler prerequisite #532](https://github.com/wove-lang/wove/issues/532) · [glibc program names](https://sourceware.org/glibc/manual/latest/html_node/Error-Messages.html)

## Bundled dlfcn coverage

Both drivers load `dlfcn.h.wove`. The installed-compiler fixture opens
a library, releases the handle, and verifies that `dlerror` consumes the
loader error and leaves the copied message readable. It also exercises
the native null filename form of `dlopen`.

Clang exposed the same members for each platform across the macOS 27.2
SDK and Debian glibc 2.41 target matrix. Clang prints the Darwin preflight
result as `_Bool` before C23 and `bool` in C23.
[LLVM libc dlfcn checklist](https://libc.llvm.org/headers/dlfcn.html)

| Members | Coverage or outstanding contract |
| --- | --- |
| `dlopen`, `dlclose`, `dlerror` | The bridge owns the loader reference until explicit or lexical close. The error snapshot copies borrowed bytes from the current thread's diagnostic. |
| `RTLD_LAZY`, `RTLD_NOW`, `RTLD_LOCAL`, `RTLD_GLOBAL`, `RTLD_NOLOAD`, `RTLD_NODELETE` | Driver feature flags expose the integer modes on both SDKs. The fixture passes each flag through the loader API. |
| `dlopen_preflight`, `RTLD_FIRST` | Darwin-only. The fixture checks a missing and a linked path and opens the loaded image with the first-image flag. |
| `dlmopen`, `Lmid_t`, `LM_ID_BASE`, `LM_ID_NEWLM` | glibc namespace selection with an owned handle result. The fixture uses the base namespace and checks the new-namespace sentinel. |
| `RTLD_BINDING_MASK`, `RTLD_DEEPBIND`, `RTLD_DI_*`, `RTLD_DL_LINKMAP`, `RTLD_DL_SYMENT` | glibc integer flags and request codes. The installed fixture evaluates each selected name. |
| `DLFO_STRUCT_HAS_EH_COUNT`, `DLFO_STRUCT_HAS_EH_DBASE`, `DLFO_EH_SEGMENT_TYPE` | glibc exposes the two record field flags on both audited ABIs. The fixture verifies both are zero. The segment-type macro needs the ELF segment constant from `<elf.h>` at the point of expansion. |
| `RTLD_DEFAULT`, `RTLD_NEXT`, `RTLD_SELF`, `RTLD_MAIN_ONLY` | The native pointer sentinels need a borrowed pointer value contract. `RTLD_SELF` and `RTLD_MAIN_ONLY` are Darwin-only. |
| `dlsym`, `dlvsym` | Typed symbol addresses must keep the loaded object live. A null value needs to remain distinct from a loader diagnostic. |
| `dladdr`, `dladdr1`, `dlinfo`, `Dl_info`, `Dl_info_t`, `Dl_serinfo`, `Dl_serpath`, `dl_find_object` | Native address and flexible-array fields and request-specific pointer outputs need exact compiler contracts. The provider records the reserved glibc profiling and object-inspection extensions. |

## Bundled fcntl coverage

The hosted provider is loaded from `fcntl.h.wove` in both compiler inventories.
The installed fixture verifies the twelve newly covered shared constants, including
the distinct Darwin and glibc values for lease and open-file-description lock commands.

Clang exposed 127 object-like macros and one function-like macro in the audited Darwin
fcntl families, and 125 object-like macros in Debian glibc 2.41. Macro names were stable in
C11, C17, and C23 on both architectures. On glibc, `O_DIRECTORY`, `O_DIRECT`,
`O_NOFOLLOW`, and `O_TMPFILE` have architecture-specific values. Darwin's
`O_POPUP` and glibc's `DN_MULTISHOT` have unsigned native expression types.

| Members | Coverage or outstanding contract |
| --- | --- |
| `O_ASYNC`, `O_FSYNC`, `O_NDELAY`, `LOCK_EX`, `LOCK_NB`, `LOCK_SH`, `LOCK_UN` | Expose and verify the shared legacy open flags and BSD advisory-lock constants with their native values. |
| `F_GETLEASE`, `F_SETLEASE`, `F_OFD_GETLK`, `F_OFD_SETLK`, `F_OFD_SETLKW` | Expose and verify the command numbers on both SDKs. The command-specific variadic calls remain pending. |
| Darwin open, path, allocation, and descriptor commands | The fixture checks 72 Darwin-specific constants and the `F_SETLEASE_ARG` encoder. The provider keeps the native unsigned type of `O_POPUP`. These spellings also appear in the earlier public XNU header. [XNU fcntl.h](https://github.com/apple-oss-distributions/xnu/blob/xnu-11215.1.10/bsd/sys/fcntl.h) |
| `AT_NODELETEBUSY`, `AT_RESOLVE_BENEATH`, `AT_UNIQUE`, `F_DUPFD_CLOFORK`, `F_RDADVISEV`, `F_RDADVISEV_NOAGE`, `O_RESOLVE_BENEATH`, `O_UNIQUE` | All eight constants are exposed as optional `i32` on Darwin. macOS SDK 27.2 supplies each in C11, C17, and C23 on both architectures. The fixture checks each native value and the import diagnostic on SDKs without the definition. [Compiler prerequisite #539](https://github.com/wove-lang/wove/issues/539) |
| glibc path, directory notification, allocation, seal, descriptor, and splice flags | The fixture checks 73 constants, including unsigned `DN_MULTISHOT` and the architecture-specific values of `O_DIRECT` and `O_TMPFILE`. The glibc re-exports `F_OK`, `R_OK`, `W_OK`, and `X_OK` are provided by `unistd.wove`. [glibc fcntl constants](https://github.com/bminor/glibc/blob/glibc-2.39/sysdeps/unix/sysv/linux/bits/fcntl-linux.h) · [Linux allocation flags](https://github.com/torvalds/linux/blob/v6.8/include/uapi/linux/falloc.h) |
| `F_CREATED_QUERY`, `F_DUPFD_QUERY` | Exposed as optional `i32` on Linux. Debian glibc 2.41 declares both query command macros in C11, C17, and C23 on both architectures. [Compiler prerequisite #539](https://github.com/wove-lang/wove/issues/539) |
| `FALLOC_FL_ALLOCATE_RANGE` | Exposed as optional `i32` on Linux. Debian glibc 2.41 includes Linux UAPI `<linux/falloc.h>` with GNU features. The audited UAPI 6.12.118 headers provide the zero-valued flag in all three C modes on both architectures. [Compiler prerequisite #539](https://github.com/wove-lang/wove/issues/539) |
| `FALLOC_FL_NO_HIDE_STALE` | Linux reserves the codepoint. |
| `creat64`, `open64`, `openat64`, `fcntl64`, `lockf64`, `posix_fadvise64`, `posix_fallocate64`, `fallocate`, `fallocate64` | Use explicit `off64_t` offsets and the exact variadic prefix for file opens and the descriptor-flag query. The fixture verifies descriptor opens and large-file operations. |
| `readahead`, `sync_file_range`, `tee`, `splice` | Glibc provides exact byte counts and offsets. The signed byte-count result shares the public `ssize_t` from `<unistd.h>`. `splice` accepts mutable offset slots and null offset pointers. |
| `vmsplice`, `name_to_handle_at`, `open_by_handle_at` | Native `iovec` slices and the variable-sized `file_handle` storage require explicit layout, extent, and ownership contracts. |
| `filesec_t`, `filesec_init`, `filesec_dup`, `filesec_free`, `openx_np`, `flock` | Darwin exposes owned security property handles, both borrowed and fixed-null extended opens, and descriptor advisory locks. The fixture opens a file through both calls and verifies independent security handles, lock contention, and reacquisition after unlocking. |
| `F_GETNOSIGPIPE`, `F_SETNOSIGPIPE`, `F_NOCACHE`, `F_RDAHEAD` | Darwin uses a fixed query or an exact integer variadic operand. The fixture changes both file-cache controls and verifies SIGPIPE suppression through the setting and an `EPIPE` write. [XNU fcntl manual](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/man/man2/fcntl.2) |
| `filesec_get_property`, `filesec_query_property`, `filesec_set_property`, `filesec_unset_property` | Model the property-specific native payload, size, and ownership through the appropriate selector. |
| GNU `flock64`, `f_owner_ex`, `file_handle`, Darwin file-security and fcntl records | Preserve their native field layouts, applicable storage and ownership contracts, and the command-specific `fcntl` operands. |

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

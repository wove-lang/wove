<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Bootstrap driver guards

The Python seed builds the bootstrap driver, which takes a single source
entry and writes one C translation unit to stdout. The bootstrap and native
drivers share graph checking, embed planning, and C emission. They select
different I/O and publication operations around that shared work.

The audit at `467a5c04` found 30 guards across five source files, including
11 positive and 19 negated guards. The cleanup removes four, leaving
10 positive and 16 negated guards.

## Entry and graph emission

| Guarded use | Selection | Disposition |
| --- | --- | --- |
| [`wovec.wove`](../../wovec.wove) imports `run_bootstrap_command` | Bootstrap | Keep the private one-entry stdout command and its fixed host ABI. |
| `wovec.wove` imports `run_native_command` | Native | Keep the installed command parser, project and profile settings, and output commands. |
| [`graph_emit.wove`](../graph_emit.wove) imports `stage_library_abi` | Native | Keep public header and export-list staging on the library publication path. |
| `graph_emit.wove` imports `stage_checked_units` | Native | Keep numbered C units and their Clang include checks in the native build. |
| `graph_emit.wove` imports `check_header_inputs` and `check_asset_inputs` | Native | Keep destination-alias checks on native builds that may write files. |
| `graph_emit.wove` imports `publish_input` | Native | Keep retained-file and staged-file publication in the native driver. |
| `graph_emit.wove` imports `BuildProtection` | Native | Keep the native target, retained tree, and library header identities. Its `FileIdentity` members use a nested record with `u64` fields outside the seed record subset. |
| `graph_emit.wove` imports `check_optional_c_members` | Native | Keep optional header-member probes with the configured native Clang. |
| `graph_emit.wove` imports `check_numeric_targets` | Native | Keep target-specific floating-point and wide-integer capability checks with the configured native toolchain. |
| `graph_emit.wove` imports `directory_width` | Native | Make the import unconditional. It reads a byte view and returns `i32`, and the bootstrap loader already uses the same function. |
| `graph_emit.wove` defines `BuildProtection` | Bootstrap | Keep the empty type used by the shared emitter signature. Stdout has no destination identity table. |
| `graph_emit.wove` defines `check_entry_inputs` | Native | Keep the implementation that checks headers and assets against destinations before publication. |
| `graph_emit.wove` defines `check_entry_inputs` | Bootstrap | Keep the success stub for the stdout path. |
| `graph_emit.wove` defines `publish_entry` | Native | Keep the filesystem publication path and its diagnostic status. |
| `graph_emit.wove` defines `publish_entry` | Bootstrap | Keep the `bootstrap_write` stdout path and its write-failure status. |
| `graph_emit.wove` defines `stage_library_abi` | Bootstrap | Keep the stub that declines library ABI staging on the stdout path. |
| `graph_emit.wove` defines `stage_checked_units` | Bootstrap | Keep the stub for the shared emitter. The bootstrap entry supplies a zero partition and emits one unit. |
| `graph_emit.wove` defines `check_entry_frontend` | Native | Keep numeric checks and optional-member queries in their existing error order. |
| `graph_emit.wove` defines `check_entry_frontend` | Bootstrap | Keep the success stub. The bootstrap emission step has no configured Clang frontend. |
| `graph_emit.wove` defines `emit_bootstrap_entry` | Bootstrap | Keep the adapter that selects the canonical entry and supplies stdout settings to the shared emitter. |

## Assets, paths, and hashing

| Guarded use | Selection | Disposition |
| --- | --- | --- |
| [`assets.wove`](../assets.wove) imports `join_file_path` | Native | Keep the native alias for the allocating path wrapper. The seed cannot select its owned-array result. |
| `assets.wove` imports `file_stamp` and native `read_file_range` | Native | Keep the POSIX backend, which matches device, inode, and length before filling the reserved asset range. |
| `assets.wove` imports `write_file_path` | Bootstrap | Make the import unconditional. It measures and fills a caller-provided byte view using scalar results. |
| `assets.wove` imports `measure_file` and bootstrap `read_file_range` | Bootstrap | Keep the backend selected through `bootstrap_file` and its exact extent check. |
| `assets.wove` defines `copy_source_prefix` | Native | Move it to its sole caller's module, [`source.wove`](../source.wove). The seed can emit this helper, but its C leaves the local copy result unused under `-Werror`. Relocation keeps the seed emitter unchanged. |
| `assets.wove` defines `asset_io` | Native | Keep the native allocating path and descriptor identity checks. |
| `assets.wove` defines `asset_io` | Bootstrap | Keep caller-owned path buffers and the fixed bootstrap read calls. Both implementations fill the same checked asset ranges. |
| [`paths.wove`](../paths.wove) defines `join_file_path` | Native | Keep the guard on `(returns (owned-array u8))`. Seed functions return a copyable value or `void`. The shared `write_file_path` already covers size and copy in the bootstrap path. |
| [`base32hex.wove`](../../hashing/base32hex.wove) defines `digest_base32hex_width` | Native | Remove the guard. The body returns an `i32` constant. Regenerate the probe function aliases after the added seed declaration changes their numbering. |
| `base32hex.wove` defines `digest_base32hex` | Native | Keep the guard on `(array u32 8)` and `(owned-array u8)`. The seed rejects the fixed array at parameter reading. A separate probe rejects an owned-array result at result reading. |

Each candidate was tested independently with the Python seed and strict C11
checking. The two import changes produced byte-identical seed C on their own.
The scalar width helper compiled with warnings treated as errors. Unguarding
the two native signatures reproduced the seed errors at the declared type
forms. The larger signature extensions stay outside this cleanup.

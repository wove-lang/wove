<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# GCC header inspector

`inspect.cc` is the standalone GCC plugin ported from [the archived inspector](https://github.com/wove-lang/.0.wove/blob/8ac13c06c422a2e2e9c66f59ec1a54f1fe106e52/stage-2/gcc/inspect.cc).
It writes one JSON object per C translation unit. Its top-level `inner` array
contains named function, variable, and typedef declarations and `wvpr_` enum
probes. Enum values retain GCC's full-width decimal text as JSON strings.
The Stage 2 AST lookup used for Clang output also reads this output.

`make test-stage-2-gcc` reads the checked-in Debian arm64 GCC 12.2.0 capture
and runs as part of `make test`. To build and load the plugin, run
`make test-stage-2-gcc-live GCC=gcc-12 GCC_CXX=g++-12` with GNU GCC, G++, and
the plugin headers from that GCC build. The live test renders the Stage 2 C
header probe, runs GCC with the plugin, and checks the resulting JSON.

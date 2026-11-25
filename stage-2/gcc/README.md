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

`query_gcc_header` discovers the selected GCC's plugin include directory,
materializes the embedded inspector in a private workspace, and uses that
GCC's C++ frontend to build the plugin. It passes the caller's packed target
and include arguments to the C probe. The inspector is built with the
compiler's default host options so probe flags such as `-funsigned-char`
affect the C translation unit.

The query returns the same presence state, AST index, and import span as
`query_clang_header`. Error 8 means plugin directory discovery failed.
Error 9 means plugin compilation failed or did not produce a regular
shared object. The shared query codes cover tool and plugin load failures,
capture truncation, and malformed AST output. The private source and plugin
are removed before the result is returned.

`make test-stage-2-gcc` checks the Debian arm64 GCC 12.2.0 capture and exercises
Wove's discovery, build, invocation, and cleanup with a controlled tool.
It runs as part of `make test`. To build and load the real plugin, run
`make test-stage-2-gcc-live GCC=gcc-12 GCC_CXX=g++-12` with GNU GCC, G++, and
the plugin headers from that GCC build. The live tests run the standalone
inspector and query headers end to end through Wove.

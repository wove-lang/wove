<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 1 seed

The Python seed compiles the [bootstrap language subset](../stage-2/LANGUAGE-SUBSET.md)
that Stage 2 is written in. It reads an entry module and its local imports,
writes one C11 translation unit to stdout, and reports source errors by file
and line.

```sh
python3 stage-1/wovec.py SOURCE
make stage-1
```

The generated C includes `wove_runtime.h` and links with
`runtime/wove_runtime.c`. `make test` runs the seed cases in the
[case manifest](../tests/stage-1/cases.tsv).

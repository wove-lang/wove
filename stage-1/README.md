<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 1 seed

The Python seed lowers the [subset](../stage-2/LANGUAGE-SUBSET.md) used by
the shared Wove graph checker and C emitter. The build then uses that emitter
to compile [`stage-2/wovec.wove`](../stage-2/wovec.wove) and its imports.

```sh
make stage-1
```

The resulting `build/stage-1/wovec` uses the ordinary driver and runtime.
Make invokes its `bin` command to build the next compiler generation.
The seed's standalone lowering cases are in the [case manifest](../tests/stage-1/cases.tsv).

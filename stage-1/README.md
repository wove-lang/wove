<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Stage 1 seed

The Python seed directly lowers [`stage-2/wovec.wove`](../stage-2/wovec.wove)
and its imports to C. It supports the [subset](../stage-2/LANGUAGE-SUBSET.md)
needed by the compiler sources.

```sh
make stage-1
```

The resulting `build/stage-1/wovec` uses the ordinary driver and runtime.
Make invokes its `bin` command to build the next compiler generation.
The seed's standalone lowering cases are in the [case manifest](../tests/stage-1/cases.tsv).

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Cross-unit callers all link to this single definition.
int bridge_shared(int value) { return value + 1; }

// Only the root calls this symbol.
int bridge_root(void) { return 10; }

// Only the helper module calls this symbol.
int bridge_child(void) { return 13; }

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

extern int native_found;

// Neither the Wove name nor a native name with a common prefix can satisfy the selection.
static inline int available(void) { return 10; }
static inline int native_found_extra(void) { return 20; }

// An ordinary selection remains usable when the optional function is absent.
static inline int regular(void) { return 42; }
static inline int companion(void) { return 2; }

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

static int bridge_counter;

// The two Wove callers must see this same counter.
static inline int bridge_next(void) { return ++bridge_counter; }

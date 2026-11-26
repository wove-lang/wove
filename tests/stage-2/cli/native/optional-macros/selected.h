// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define DIRECT (6 + 7)
#define ALIAS DIRECT
#ifdef ENABLE_OPTIONAL_MACRO
#define CONDITIONAL (ALIAS + 2)
#endif

// Optional functions can share the provider with optional macros.
static inline int increment(void) { return 1; }

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifdef ENABLE_NATIVE
// Use the original native spelling for the renamed Wove function.
static inline int native_found(void) { return 40; }
#endif

// The second optional selection uses the same header observation.
static inline int companion(void) { return 2; }

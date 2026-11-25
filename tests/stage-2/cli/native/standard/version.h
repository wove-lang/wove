// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// The selected C standard must expose the same declaration to the probe and build.
#if (defined(WOVE_DIALECT_C11) && __STDC_VERSION__ == 201112L) || \
    (defined(WOVE_DIALECT_C17) && __STDC_VERSION__ == 201710L) || \
    (defined(WOVE_DIALECT_C23) && __STDC_VERSION__ >= 202000L)
static inline int native_found(void) { return 40; }
#endif

// One selected declaration is available in every supported dialect.
static inline int companion(void) { return 2; }

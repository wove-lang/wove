// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Neither feature is enabled by the default native frontend options.
#if !defined(_REENTRANT) && !defined(_XOPEN_SOURCE)
static inline int native_found(void) { return 40; }
#endif

// This selection is present in both frontend configurations.
static inline int companion(void) { return 2; }

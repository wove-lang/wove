// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// The default frontend flags and the private runtime must not alter this header.
#if !defined(_REENTRANT) && !defined(_XOPEN_SOURCE) && !defined(WVRT_RUNTIME_H)
static inline int native_found(void) { return 40; }
#endif

// This selection is present in both frontend configurations.
static inline int companion(void) { return 2; }

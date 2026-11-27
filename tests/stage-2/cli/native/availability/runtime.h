// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Verify the runtime header is visible through the private staging include path.
#if !__has_include("wove_runtime.h")
#error "the private staging directory must provide wove_runtime.h"
#endif

static inline int native_found(void) { return 40; }
static inline int companion(void) { return 2; }

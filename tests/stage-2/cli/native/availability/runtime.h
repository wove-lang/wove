// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

// Both frontend invocations must find the runtime header through the private stage.
static inline int native_found(void) { return 40; }
static inline int companion(void) { return 2; }

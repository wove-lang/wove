// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_WORDS_H
#define WOVE_TEST_WORDS_H

#include <stdint.h>

// Preserve the exact signed 64-bit value across the C boundary.
static inline int64_t keep_i64(int64_t value) {
	return value;
}

#endif

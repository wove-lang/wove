// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TARGET_PLATFORM_H
#define WOVE_TEST_TARGET_PLATFORM_H

#include <stdint.h>

// Accept both target variants of the fixture's small integer argument.
static inline int32_t target_offset(uint32_t offset) {
	return (int32_t)offset;
}

#endif

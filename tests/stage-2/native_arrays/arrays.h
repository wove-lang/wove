// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_ARRAYS_H
#define WOVE_TEST_NATIVE_ARRAYS_H

#include <stdint.h>

// Write values whose upper bits distinguish their native stride and width.
static inline int32_t write_wide(uint64_t *data, int32_t count) {
	if (count != 2) { return 1; }
	data[0] = UINT64_C(1099511627781);
	data[1] = UINT64_C(562949953421319);
	return 0;
}

// Read both elements through the header's const pointer.
static inline int32_t read_wide(const uint64_t *data) {
	return data[0] == UINT64_C(1099511627781) && data[1] == UINT64_C(562949953421319) ? 0 : 1;
}

// Passing an empty view with a zero element count requires no dereference.
static inline int32_t empty_wide(const uint64_t *data, uint64_t count) {
	(void)data;
	return count == 0 ? 0 : 1;
}

#endif

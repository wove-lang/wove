// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_ALIAS_H
#define WOVE_TEST_NATIVE_ALIAS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Supply a value that exercises the full native size_t width.
static inline size_t wide_count(void) {
	return SIZE_MAX;
}

// Supply the same C representation through a distinct Wove type declaration.
static inline size_t distinct_count(void) {
	return SIZE_MAX;
}

// Observe the exact value after it has passed through Wove locals and calls.
static inline int32_t check_count(size_t value) {
	return value == SIZE_MAX ? 42 : 0;
}

// Return a full-width result only if the empty Wove slice was NUL-terminated.
static inline size_t wide_text_count(const char *text) {
	return text[0] == '\0' ? SIZE_MAX : 0;
}

// Observe the checked count and read from the original byte storage.
static inline int32_t check_byte_count(const void *data, size_t count) {
	if (count != 1) { fputs("entered C with an unchecked count\n", stderr); return 1; }
	return ((const uint8_t *)data)[0] == 0 ? 42 : 2;
}

// Observe the original full-width operands even when their product is zero.
static inline int32_t check_byte_product(const void *data, size_t size, size_t count) {
	if ((size == 0 && count == SIZE_MAX) || (count == 0 && size == SIZE_MAX)) {
		return 0;
	}
	fputs("entered C with an unchecked product\n", stderr);
	return data != NULL ? 1 : 2;
}

// Mark any entry, since a negative factor should be rejected before the call.
static inline int32_t check_signed_product(const void *data, long size, size_t count) {
	fputs("entered C with a negative factor\n", stderr);
	return data != NULL && size < 0 && count == 0;
}

#endif

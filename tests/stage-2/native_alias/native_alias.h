// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_ALIAS_H
#define WOVE_TEST_NATIVE_ALIAS_H

#include <stddef.h>
#include <stdint.h>

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

#endif

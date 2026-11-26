// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_NATIVE_FIXED_VALUES_H
#define WOVE_NATIVE_FIXED_VALUES_H

#include <stdint.h>

#define FIXED_TAG 7

// Check all hidden operands at their exact native argument positions.
static inline int32_t check_fixed(int32_t first, int32_t minimum, const char **tail,
	int32_t hexadecimal, int32_t tag, int32_t maximum, int32_t last) {
	return first == 11 && minimum == INT32_MIN && tail == 0 && hexadecimal == -16 &&
		tag == FIXED_TAG && maximum == INT32_MAX && last == 31 ? first + last : 0;
}

// Check a hidden prefix followed by an empty variadic tail.
static inline int32_t check_empty_tail(const char **tail, int32_t code, ...) {
	return tail == 0 && code == -1;
}

// The bridge's const char ** declaration must disagree with this signature.
static inline int32_t wrong_tail(void **tail) {
	return tail == 0;
}

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_C_BYTES_H
#define WOVE_TEST_C_BYTES_H

#include <stdint.h>
#include <stdio.h>

static uintptr_t wove_test_bytes_origin;
static int32_t wove_test_bytes_order;

// Make the order of the actual count and size operands observable.
static inline int32_t wove_test_byte_step(int32_t order, int32_t value) {
	if (wove_test_bytes_order++ != order) { return -1; }
	return value;
}

// Write only the checked prefix of the original five-byte allocation.
static inline int32_t wove_test_write_bytes(uint8_t *data, uint8_t count, int32_t size) {
	if (count != 2 || size != 2 || wove_test_bytes_order != 2 || data[4] != 0) { return 1; }
	wove_test_bytes_origin = (uintptr_t)data;
	data[0] = 7;
	data[1] = 255;
	data[2] = 42;
	data[3] = 11;
	return 0;
}

// The bounded input is the original subview while text is a separate C string.
static inline int32_t wove_test_read_bytes(int32_t count, const uint8_t *input, const char *text) {
	// A zero-length view at the end still points at the end of the original storage.
	if (count == 0) {
		return (uintptr_t)input == wove_test_bytes_origin + 5 && text[0] == 0 ? 0 : 2;
	}
	return count == 2 && (uintptr_t)input == wove_test_bytes_origin + 1 && input[0] == 255 &&
		input[1] == 42 && text[0] == 42 && text[1] == 11 && text[2] == 0 ? 42 : 3;
}

// A zero size accepts a full-width unsigned count without touching storage.
static inline int32_t wove_test_zero_bytes(const uint8_t *data, uint32_t size, uint32_t count) {
	(void)data;
	return size == 0 && count == UINT32_MAX ? 0 : 4;
}

// Print a marker if an invalid extent reaches the C body.
static inline int32_t wove_test_trap_count(const uint8_t *data, int32_t count, int32_t last) {
	(void)data;
	(void)count;
	(void)last;
	fputs("native body ran\n", stderr);
	return 99;
}

// Print the same marker for rejected product extents.
static inline int32_t wove_test_trap_product(uint8_t *data, int32_t size, uint32_t count) {
	(void)data;
	(void)size;
	(void)count;
	fputs("native body ran\n", stderr);
	return 99;
}

// Give overflow tests two full-width unsigned dimensions.
static inline int32_t wove_test_trap_overflow(uint8_t *data, uint32_t size, uint32_t count) {
	(void)data;
	(void)size;
	(void)count;
	fputs("native body ran\n", stderr);
	return 99;
}

// A later scalar operand must finish before the extent is checked.
static inline int32_t wove_test_byte_marker(void) {
	fputs("last argument evaluated\n", stderr);
	return 0;
}

#endif

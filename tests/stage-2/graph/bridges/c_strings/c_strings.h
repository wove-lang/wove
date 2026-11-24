/*
 * Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_BRIDGE_C_STRINGS_H
#define WOVE_TEST_BRIDGE_C_STRINGS_H

#include <stdint.h>
#include <stdio.h>

static int32_t wove_test_c_string_order;

// Return the supplied value while recording which argument evaluation ran next.
static inline int32_t wove_test_c_string_step(int32_t ordinal, int32_t value) {
	if (ordinal != wove_test_c_string_order) { return -1; }
	++wove_test_c_string_order;
	return value;
}

// Observe every copied byte and both nonnull empty arguments during the native call.
static inline int32_t wove_test_observe_c_strings(const char *text, const char *empty_checked,
		int32_t marker, const char *raw, const char *empty_raw) {
	return text && empty_checked && raw && empty_raw &&
		text[0] == 'A' && (unsigned char)text[1] == 255 && text[2] == '\0' &&
		empty_checked[0] == '\0' && raw[0] == 'B' && raw[1] == '\0' &&
		raw[2] == 'C' && raw[3] == '\0' && empty_raw[0] == '\0' &&
		marker == 7 && wove_test_c_string_order == 3 ? 42 : 1;
}

// Make evaluation of the later scalar operand visible before the checked trap.
static inline int32_t wove_test_c_string_later(void) {
	fputs("later argument evaluated\n", stderr);
	return 7;
}

// Reaching this native body would add a line the checked-trap fixture rejects.
static inline int32_t wove_test_enter_c_string_trap(const char *raw, const char *checked, int32_t marker) {
	(void)raw;
	(void)checked;
	(void)marker;
	fputs("native body ran\n", stderr);
	return 1;
}

#endif

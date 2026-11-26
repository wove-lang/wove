/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#ifndef WOVE_TEST_HALF_H
#define WOVE_TEST_HALF_H

/* Match the Clang capability probe. */
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
typedef struct {
	_Float16 value;
} HalfPacket;

/* Exercise a half argument and return value through the C ABI. */
static inline _Float16 half_twice(_Float16 value) {
	return value + value;
}

/* Store the native value at the layout used by the declared bridge field. */
static inline int half_store(HalfPacket *out, _Float16 value) {
	out->value = value;
	return 0;
}
#endif

#endif

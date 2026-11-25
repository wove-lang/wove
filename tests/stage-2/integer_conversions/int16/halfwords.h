// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_HALFWORDS_H
#define WOVE_TEST_HALFWORDS_H

#include <stdint.h>

// Pass an unsigned value across an exact-width native signature.
static inline uint16_t keep_u16(uint16_t value) {
	return value;
}

// Pass a signed value across an exact-width native signature.
static inline int16_t keep_i16(int16_t value) {
	return value;
}

#endif

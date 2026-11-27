/*
 * Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_GRAPH_BRIDGE_LOCAL_H
#define WOVE_TEST_GRAPH_BRIDGE_LOCAL_H

#include <stdbool.h>
#include <stdint.h>

uint8_t value(uint32_t sample, bool enabled);

static int32_t bridge_digits;

// Record the call order and return the state observed by this argument.
static inline int32_t bridge_next(int32_t digit) {
	bridge_digits = bridge_digits * 10 + digit;
	return bridge_digits;
}

// Accept only the argument values and shared state from one left-to-right evaluation.
static inline int32_t bridge_verify(int32_t first, int32_t second) {
	return first == 4 && second == 42 && bridge_digits == 42 ? second : 1;
}

// Expose a native spelling that is also the default name of a Wove temporary.
static inline int32_t wgt0000024(int32_t first, int32_t second) {
	return bridge_verify(first, second);
}

// Occupy the default file-scope name of the Wove helper.
static inline int32_t wgf0igfllvnumber(void) {
	return 22;
}

// Occupy the first fallback even though the Wove program only selects this function.
static inline int32_t wwgf0igfllvnumber(void) {
	return 1;
}

#endif

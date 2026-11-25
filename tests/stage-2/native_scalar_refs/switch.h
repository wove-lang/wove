// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_SCALAR_SWITCH_H
#define WOVE_TEST_NATIVE_SCALAR_SWITCH_H

#include <stdbool.h>
#include <stdint.h>

// Report a null argument separately and set the supplied boolean in place.
static inline int32_t set_switch(bool *value) {
	if (value == 0) { return 7; }
	*value = true;
	return 0;
}

#endif

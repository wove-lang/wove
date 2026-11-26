// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_RECORD_INIT_INNER_H
#define WOVE_RECORD_INIT_INNER_H

#include <stdint.h>

struct wove_inner {
	int32_t word;
	int32_t private_word;
};

static int32_t nested_calls;

// Return consecutive values so the parent constructor's evaluation order is observable.
static inline int32_t next_inner_word(void) { return ++nested_calls * 11; }

#endif

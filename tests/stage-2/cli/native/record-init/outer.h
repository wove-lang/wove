// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_RECORD_INIT_OUTER_H
#define WOVE_RECORD_INIT_OUTER_H

#include "./inner.h"
#include <stddef.h>

struct wove_outer {
	struct wove_inner first;
	struct wove_inner second;
	struct wove_inner supplied;
	struct wove_inner omitted;
	void *private_pointer;
};

// Check both the declared field values and omitted storage in both native layouts.
static inline int32_t verify_outer(const struct wove_outer *value) {
	return nested_calls == 2 && value->first.word == 22 && value->second.word == 11 &&
		value->supplied.word == 7 && value->omitted.word == 0 && value->omitted.private_word == 0 &&
		value->first.private_word == 0 && value->second.private_word == 0 &&
		value->supplied.private_word == 0 && value->private_pointer == NULL ? 42 : 1;
}

#endif

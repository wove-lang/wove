// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

static inline int fixture_dispatch(int value, int (*receiver)(const char *, int)) {
	return receiver(value < 0 ? NULL : "walk", value);
}

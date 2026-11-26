// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>

__extension__ typedef unsigned __int128 fixture_u128;
__extension__ typedef __int128 fixture_i128;

// Keep the value observable if an invalid extent crosses the bridge.
static inline int32_t wide_bytes(const uint8_t *data, fixture_i128 size, fixture_u128 count) {
	(void)size;
	(void)count;
	return *data;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// The replaced tile and truncated tile drop first. Scope cleanup then drops
// the removed tile, moved leaf, partially populated tile, and surviving tile.
static const size_t wove_track_drop_order[] = {
	12, 11, 10, 9, 20, 19, 18, 17, 16, 15, 14, 13, 3, 4, 2, 1, 8, 7, 6, 5, 0
};
#define WOVE_TRACK_GROWN_ARRAY 0
#define WOVE_TRACK_GROWN_CAPACITY 6
#include "../../support/array_order_tracking.h"

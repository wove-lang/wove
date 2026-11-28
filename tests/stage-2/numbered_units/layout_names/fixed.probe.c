// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// The callee drops the removed left element before the caller drops the right heap and the emptied left heap.
static const size_t wove_track_drop_order[] = {2, 3, 1, 0};
#include "../../../support/array_order_tracking.h"

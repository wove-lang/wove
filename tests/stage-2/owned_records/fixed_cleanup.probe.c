// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// Replacement retires one pair. Direct rows and the heap envelope drop next,
// then the transferred bundles and the last live slot in the original array.
static const size_t wove_track_drop_order[] = {5, 4, 8, 9, 12, 11, 10, 7, 6, 1, 0, 3, 2};
#include "../../support/array_order_tracking.h"

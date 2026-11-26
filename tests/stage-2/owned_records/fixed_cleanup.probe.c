// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// Replacement retires one pair. The direct rows, heap envelope, and selected
// arms drop next. Transferred bundles and the last original slot drop at exit.
static const size_t wove_track_drop_order[] = {5, 4, 8, 9, 12, 11, 10, 13, 14, 7, 6, 1, 0, 3, 2};
#include "../../support/array_order_tracking.h"

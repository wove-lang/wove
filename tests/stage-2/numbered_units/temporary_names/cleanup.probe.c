// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// Each inner break drops its array before the outer break drops the next one.
static const size_t wove_track_drop_order[] = {1, 0, 3, 2};
#include "../../../support/array_order_tracking.h"

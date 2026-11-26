// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// The swap leaves two live owners. Each refill drops the old owner and then its replacement.
static const size_t wove_track_drop_order[] = {1, 0, 2, 3, 4, 5, 6, 7, 8, 9};
#include "../../support/array_order_tracking.h"

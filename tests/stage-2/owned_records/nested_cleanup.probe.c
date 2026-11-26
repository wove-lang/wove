// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// A moved subrecord drops both fields as a separate local. After a leaf
// move the outer record drops the intact sibling, then the remaining leaf.
static const size_t wove_track_drop_order[] = {1, 0, 3, 2, 4, 7, 6, 5};
#include "../../support/array_order_tracking.h"

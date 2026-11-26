// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// Replacement and truncation drop their records immediately. The removed
// record is a separate local, then the populated array slot and its owner drop.
static const size_t wove_track_drop_order[] = {2, 1, 8, 7, 4, 3, 6, 5, 0};
#include "../../support/array_order_tracking.h"

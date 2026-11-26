// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// A replacement drops its previous row immediately. Whole-record cleanup
// follows reverse field order. A moved group drops through its local owners.
static const size_t wove_track_drop_order[] = {2, 0, 4, 3, 5, 1, 8, 10, 9, 11, 6, 7};
#include "../../support/array_order_tracking.h"

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// Each discard drops spare before values. The last callee moves values
// to a local, which is released before the incoming packet's spare.
static const size_t wove_track_drop_order[] = {1, 0, 3, 2, 4, 5};
#include "../../support/array_order_tracking.h"

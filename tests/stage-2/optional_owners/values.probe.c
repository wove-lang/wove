// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// The absent result allocates nothing. The unwrapped and retained owners drop in reverse order.
static const size_t wove_track_drop_order[] = {2, 1, 0};
#include "../../support/array_order_tracking.h"

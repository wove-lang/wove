// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// The moved record drops the nested descriptors. The fixed owner and moved
// slot drop next, then the original aggregates drop their remaining members.
static const size_t wove_track_drop_order[] = {6, 5, 4, 2, 3, 0, 1};
#include "../../support/array_order_tracking.h"

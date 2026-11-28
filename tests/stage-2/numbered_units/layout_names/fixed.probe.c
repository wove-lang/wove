// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// The callees drop both removed elements in call order, then the caller drops the two emptied heaps in reverse order.
static const size_t wove_track_drop_order[] = {2, 3, 1, 0};
#include "../../../support/array_order_tracking.h"

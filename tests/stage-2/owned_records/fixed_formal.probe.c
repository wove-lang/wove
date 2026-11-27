// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// The early calls drop complete arrays in reverse element order. The
// fallthrough path forwards one slot and then drops the remaining slot.
// Finally the restored array drops in the callee and the original slot drops locally.
static const size_t wove_track_drop_order[] = {1, 0, 3, 2, 4, 5, 7, 8, 6};
#include "../../support/array_order_tracking.h"

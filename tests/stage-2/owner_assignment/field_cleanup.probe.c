// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>

// Each rebuild retires one slot. Scope exit retires the sibling followed by the restored slot.
static const size_t wove_track_drop_order[] = {0, 2, 1, 3};
#include "../../support/array_order_tracking.h"

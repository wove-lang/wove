// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#undef main

// The cursor and its field reborrows share the single backing allocation.
#define WOVE_TRACK_ARRAYS 1
#define WOVE_TRACK_ELEMENTS 3
#define WOVE_TRACK_ELEMENT_SIZE 1
#include "../../support/array_tracking.h"

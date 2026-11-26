// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#undef main

// The scoped payload releases the Wove snapshot once after the C source is released.
#define WOVE_TRACK_ARRAYS 1
#define WOVE_TRACK_ELEMENTS 2
#define WOVE_TRACK_ELEMENT_SIZE 1
#include "../../support/array_tracking.h"

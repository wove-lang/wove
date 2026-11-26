// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#undef main

// Release one nested enum directly and one byte owner extracted through two matches.
#define WOVE_TRACK_ARRAYS 2
#define WOVE_TRACK_ELEMENTS 42
#define WOVE_TRACK_ELEMENT_SIZE 1
#include "../../support/array_tracking.h"

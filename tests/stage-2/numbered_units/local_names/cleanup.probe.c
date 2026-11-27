// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#undef main

// Each concrete specialization releases its own local byte array.
#define WOVE_TRACK_ARRAYS 2
#define WOVE_TRACK_ELEMENTS 1
#define WOVE_TRACK_ELEMENT_SIZE 1
#include "../../../support/array_tracking.h"

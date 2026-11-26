// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#undef main

// The two bound cases, two discards, and untouched local each release one Grid.
#define WOVE_TRACK_ARRAYS 5
#define WOVE_TRACK_ELEMENTS 42
#define WOVE_TRACK_ELEMENT_SIZE 1
#include "../../support/array_tracking.h"

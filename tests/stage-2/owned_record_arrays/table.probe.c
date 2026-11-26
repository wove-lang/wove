// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>

#undef main

typedef struct {
	uint32_t key;
	int32_t count;
} EntryLayout;

// All three helpers transfer this one allocation to the entry's cleanup.
#define WOVE_TRACK_ARRAYS 1
#define WOVE_TRACK_ELEMENTS 2
#define WOVE_TRACK_ELEMENT_SIZE sizeof(EntryLayout)
#include "../../support/array_tracking.h"

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>

// Track both return paths with the native layout of the fixture's record.
typedef struct {
	uint32_t kind;
	int32_t start;
	uint8_t marker;
} RecordLayout;

#define WOVE_TRACK_ARRAYS 2
#define WOVE_TRACK_ELEMENTS 2
#define WOVE_TRACK_ELEMENT_SIZE sizeof(RecordLayout)
#include "../../support/array_tracking.h"

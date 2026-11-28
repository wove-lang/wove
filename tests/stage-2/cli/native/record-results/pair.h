// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_NATIVE_RESULT_FIXTURE_H
#define WOVE_NATIVE_RESULT_FIXTURE_H

#include <stdint.h>

typedef struct { int32_t first, second; } Pair;
typedef struct { int32_t first, second; } OtherPair;
typedef struct Incomplete Incomplete;

Incomplete get_incomplete(void);

// Return both integers by value from their native function.
static inline Pair get_pair(int32_t first, int32_t second)
{
	return (Pair){first, second};
}

#define get_pair(first, second) ((Pair){-1, -1})

#endif

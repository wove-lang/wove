// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_MACROS_H
#define WOVE_TEST_MACROS_H

#include <stdint.h>
#include <stdlib.h>

#define MACRO_REPEAT(first, second) ((uint32_t)(first) + (first) + (second))
#define MACRO_IGNORE(value) UINT32_MAX
#define MACRO_IS_BYTE(value) _Generic((value), uint8_t: 1, default: 0)

#define MACRO_FIRST_BYTE(data) ((int32_t)(unsigned char)(data)[0])
#define MACRO_VIEW(data, count) ((count) == 1 ? (((uint8_t *)(data))[0] = 42) : 0)

// Require the first captured value twice and discard the second operand.
#define MACRO_VOID_STMT(first, ignored) do { if ((first) != 1 || (first) + (first) != 2) { abort(); } } while (0)

// Exercise the same checked void result on the ordinary C call path.
static inline void wove_test_visit(int32_t *count) {
	++*count;
}

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_VOID_POINTER_H
#define WOVE_TEST_VOID_POINTER_H

#include <stdint.h>

typedef uint16_t fixture_word;
typedef uint16_t fixture_other_word;
typedef enum { FIXTURE_COPY = 4 } fixture_operation;

static int32_t fixture_trace;

/* Record the order and number of the scalar argument evaluations. */
static inline int32_t mark(int32_t value)
{
	fixture_trace = fixture_trace * 10 + value;
	return value;
}

/* Read and write a single scalar through each qualified C object pointer. */
static inline int32_t copy_word(int32_t first, fixture_operation operation,
	const void *input, int32_t second, void *output)
{
	if (first != 1 || second != 2 || fixture_trace != 12 || operation != FIXTURE_COPY)
		return -1;
	*(fixture_word *)output = *(const fixture_word *)input;
	return 0;
}

#endif

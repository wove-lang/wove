/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_NATIVE_LONG_DOUBLE_H
#define WOVE_TEST_NATIVE_LONG_DOUBLE_H

#include <float.h>

#define NATIVE_DOUBLE_EXPRESSION ((double)0.5)
#define NATIVE_LONG_EXPRESSION (1.0L + LDBL_EPSILON)

typedef struct {
	long double value;
} NativeLongBox;

/* Preserve the exact C value through a by-value argument and result. */
static inline long double native_long_roundtrip(long double value)
{
	return value;
}

/* Read the original value through a writable native reference. */
static inline long double native_long_read(long double *value)
{
	return *value;
}

/* Return the first long double after one under round-to-nearest. */
static inline long double native_long_successor(void)
{
	return 1.0L + LDBL_EPSILON;
}

/* Supply an exact input with a nonzero fractional and integral part. */
static inline long double native_long_midpoint(void)
{
	return 1.5L;
}

/* Compare the original value without narrowing or inspecting padding. */
static inline int native_long_matches(long double value, int index)
{
	static const long double expected[] = {
		LDBL_EPSILON, LDBL_MAX, LDBL_MIN, LDBL_TRUE_MIN, 1.0L + LDBL_EPSILON,
		0.5L, 1.0L, 2.0L,
	};
	return value == expected[index];
}

/* Provide a same-spelling C function whose argument uses double. */
static inline int native_double_argument(double value)
{
	return value == 0.5;
}

/* Provide the wrong return type even on targets with equal representations. */
static inline double native_double_result(void)
{
	return 0.5;
}

#endif

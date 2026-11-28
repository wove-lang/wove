/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FLOAT_EXPRESSIONS_H
#define WOVE_TEST_FLOAT_EXPRESSIONS_H

#include <float.h>

/* Return the target header's float limits in fixture order. */
static inline float native_float_value(int index)
{
	static const float values[] = {FLT_EPSILON, FLT_MAX, FLT_MIN, FLT_TRUE_MIN};
	return values[index];
}

/* Return the target header's double limits in fixture order. */
static inline double native_double_value(int index)
{
	static const double values[] = {DBL_EPSILON, DBL_MAX, DBL_MIN, DBL_TRUE_MIN};
	return values[index];
}

#endif

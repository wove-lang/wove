/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FLOAT_LIMITS_H
#define WOVE_TEST_FLOAT_LIMITS_H

#include <float.h>

/* Read the target header's integer limits in fixture order. */
static inline int native_float_limit(int index)
{
	static const int limits[] = {
		DECIMAL_DIG, FLT_EVAL_METHOD, FLT_RADIX,
		FLT_DECIMAL_DIG, FLT_DIG, FLT_HAS_SUBNORM, FLT_MANT_DIG,
		FLT_MAX_10_EXP, FLT_MAX_EXP, FLT_MIN_10_EXP, FLT_MIN_EXP,
		DBL_DECIMAL_DIG, DBL_DIG, DBL_HAS_SUBNORM, DBL_MANT_DIG,
		DBL_MAX_10_EXP, DBL_MAX_EXP, DBL_MIN_10_EXP, DBL_MIN_EXP,
		LDBL_DECIMAL_DIG, LDBL_DIG, LDBL_HAS_SUBNORM, LDBL_MANT_DIG,
		LDBL_MAX_10_EXP, LDBL_MAX_EXP, LDBL_MIN_10_EXP, LDBL_MIN_EXP,
	};
	return limits[index];
}

/* Read the runtime rounding expression directly from the selected header. */
static inline int native_float_rounds(void)
{
	return FLT_ROUNDS;
}

#endif

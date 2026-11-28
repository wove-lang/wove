/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FENV_DARWIN_H
#define WOVE_TEST_FENV_DARWIN_H

#include <fenv.h>
#include <float.h>

/* Read the exact exception mask for the selected Darwin target. */
static inline int native_fenv_denormal_flag(void)
{
#if defined(__aarch64__)
	return FE_FLUSHTOZERO;
#else
	return FE_DENORMALOPERAND;
#endif
}

/* Multiply at runtime to produce a positive, inexact subnormal by default. */
static inline double native_fenv_subnormal(void)
{
	#pragma STDC FENV_ACCESS ON
	volatile double minimum = DBL_MIN;
	volatile double factor = 0.1;
	return minimum * factor;
}

#endif

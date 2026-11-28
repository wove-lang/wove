/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FENV_CONSTANTS_H
#define WOVE_TEST_FENV_CONSTANTS_H

#include <fenv.h>

/* Read the target header's exact common constants for the Wove comparisons. */
static inline int native_fenv_constant(int index)
{
	static const int values[] = {
		FE_ALL_EXCEPT, FE_DIVBYZERO, FE_DOWNWARD, FE_INEXACT, FE_INVALID,
		FE_OVERFLOW, FE_TONEAREST, FE_TOWARDZERO, FE_UNDERFLOW, FE_UPWARD,
	};
	return values[index];
}

#endif

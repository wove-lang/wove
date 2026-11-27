/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <float.h>

#ifdef TYPE_HEADER_VISIBLE
#define TYPE_VISIBLE 42
#endif

/* Check the original long double value after it crosses the Wove call. */
static inline int match_intrinsic(long double value)
{
	return value == 1.0L + LDBL_EPSILON ? 42 : 1;
}

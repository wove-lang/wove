/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FLOAT_SNAN_H
#define WOVE_TEST_FLOAT_SNAN_H

#include <float.h>
#include <stdint.h>
#include <string.h>

#ifdef FLT_SNAN
/* Read the native macro's bytes without evaluating a floating comparison. */
static inline uint32_t native_float_snan_bits(void)
{
	static const float value = FLT_SNAN;
	uint32_t bits;
	memcpy(&bits, &value, sizeof(bits));
	return bits;
}
#endif

#ifdef DBL_SNAN
/* Compare all bytes of the native double representation. */
static inline int native_double_snan_matches(double value)
{
	static const double expected = DBL_SNAN;
	return memcmp(&value, &expected, sizeof(value)) == 0;
}
#endif

#ifdef LDBL_SNAN
/* Compare the native signaling representation, excluding x86 extended padding. */
static inline int native_long_double_snan_matches(long double value)
{
	static const long double expected = LDBL_SNAN;
	const size_t width = LDBL_MANT_DIG == 64 ? 10 : sizeof(expected);
	return __builtin_isfpclass(LDBL_SNAN, __FPCLASS_SNAN)
		&& memcmp(&value, &expected, width) == 0;
}
#endif

#endif

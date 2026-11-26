/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#ifndef WOVE_TEST_EXTENDED_H
#define WOVE_TEST_EXTENDED_H

#include <float.h>

/* Guard the bridge with the same mantissa capability the compiler observes. */
#if LDBL_MANT_DIG == 64
#include <math.h>

typedef struct {
	unsigned char marker;
	long double value;
} ExtendedPacket;

/* Exercise native long double arguments and results. */
static __attribute__((noinline)) long double extended_twice(long double value) {
	return 2.0L * value;
}

/* Exercise a C layout with alignment padding before its long double field. */
static __attribute__((noinline)) int extended_store(ExtendedPacket *out, long double value) {
	out->marker = 7;
	out->value = value;
	return 0;
}

/* Check that Wove's integer-shaped negative zero carries its sign into C. */
static __attribute__((noinline)) int extended_negative_zero(long double value) {
	return value == 0.0L && signbit(value);
}
#endif

#endif

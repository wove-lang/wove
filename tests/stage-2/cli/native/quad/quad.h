/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#ifndef WOVE_TEST_QUAD_H
#define WOVE_TEST_QUAD_H

/* Use the target's binary128 ABI independently of the Wove runtime typedef. */
#if __LDBL_MANT_DIG__ == 113
typedef long double QuadNative;
#elif defined(__SIZEOF_FLOAT128__)
typedef __float128 QuadNative;
#endif

/* Declare the bridge only where its native representation is available. */
#if __LDBL_MANT_DIG__ == 113 || defined(__SIZEOF_FLOAT128__)
typedef struct {
	unsigned char marker;
	QuadNative value;
} QuadPacket;

/* Keep a real binary128 argument and result across the C call boundary. */
static __attribute__((noinline)) QuadNative quad_twice(QuadNative value) {
	return value + value;
}

/* Exercise a C layout with padding before an aligned binary128 field. */
static __attribute__((noinline)) int quad_store(QuadPacket *out, QuadNative value) {
	out->marker = 7;
	out->value = value;
	return 0;
}

/* Observe the sign of Wove's negative zero using the native floating value. */
static __attribute__((noinline)) int quad_negative_zero(QuadNative value) {
	return value == 0 && __builtin_signbit(value);
}
#endif

#endif

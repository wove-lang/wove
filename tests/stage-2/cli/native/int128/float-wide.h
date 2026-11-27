// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

__extension__ typedef __int128 signed_wide;
__extension__ typedef unsigned __int128 unsigned_wide;

static inline signed_wide varying_i128(signed_wide input) {
	volatile signed_wide value = input;
	return value;
}

static inline unsigned_wide varying_u128(unsigned_wide input) {
	volatile unsigned_wide value = input;
	return value;
}

static inline float varying_f32(float input) {
	volatile float value = input;
	return value;
}

static inline double varying_f64(double input) {
	volatile double value = input;
	return value;
}

#if __LDBL_MANT_DIG__ == 64
static inline long double varying_f80(long double input) {
	volatile long double value = input;
	return value;
}
#endif

#if __LDBL_MANT_DIG__ == 113
typedef long double wide_quad;
#elif defined(__SIZEOF_FLOAT128__)
typedef __float128 wide_quad;
#endif

#if __LDBL_MANT_DIG__ == 113 || defined(__SIZEOF_FLOAT128__)
static inline wide_quad varying_f128(wide_quad input) {
	volatile wide_quad value = input;
	return value;
}
#endif

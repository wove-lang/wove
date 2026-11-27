// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L || !defined(__BITINT_MAXWIDTH__) || __BITINT_MAXWIDTH__ < 256
#error C23 target does not support 256-bit integers
#endif

typedef signed _BitInt(256) wvrt_probe_signed;
typedef unsigned _BitInt(256) wvrt_probe_unsigned;

_Static_assert(sizeof(wvrt_probe_signed) == 32 && sizeof(wvrt_probe_unsigned) == 32,
	"256-bit integers require 32-byte storage");

// Volatile accesses force both directions and signednesses to reach the native linker.
#define WVRT_PROBE_FLOAT_CONVERSIONS(real_type, signed_value, unsigned_value) \
	do { \
		volatile real_type from_signed = (real_type)(signed_value); \
		volatile real_type from_unsigned = (real_type)(unsigned_value); \
		(signed_value) = (wvrt_probe_signed)from_signed; \
		(unsigned_value) = (wvrt_probe_unsigned)from_unsigned; \
	} while (0)

// Select nonconstant values and force the compiler to materialize its runtime helpers.
int main(int argc, char **argv) {
	(void)argv;
	volatile wvrt_probe_signed s = -((wvrt_probe_signed)argc << 199) - 1;
	volatile wvrt_probe_signed divisor = (wvrt_probe_signed)argc + 1;
	volatile wvrt_probe_unsigned u = ((wvrt_probe_unsigned)argc << 200) + 1;
	volatile wvrt_probe_unsigned step = (wvrt_probe_unsigned)argc + 1;
	volatile wvrt_probe_signed quotient = s / divisor, remainder = s % divisor;
	volatile wvrt_probe_unsigned unsigned_quotient = u / step, unsigned_remainder = u % step;
	volatile wvrt_probe_unsigned product = unsigned_quotient * step;
	volatile wvrt_probe_signed shift = s >> (argc & 255);
	volatile wvrt_probe_unsigned rotated = u << (argc & 255);
	volatile wvrt_probe_signed signed_value = argc & 127;
	volatile wvrt_probe_unsigned unsigned_value = (unsigned int)argc & 127;
	WVRT_PROBE_FLOAT_CONVERSIONS(float, signed_value, unsigned_value);
	WVRT_PROBE_FLOAT_CONVERSIONS(double, signed_value, unsigned_value);
	WVRT_PROBE_FLOAT_CONVERSIONS(long double, signed_value, unsigned_value);
	// Half is optional and participates where the target supports it.
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
	WVRT_PROBE_FLOAT_CONVERSIONS(_Float16, signed_value, unsigned_value);
#endif
	// An independent quad type can use different runtime conversion routines.
#if __LDBL_MANT_DIG__ != 113 && defined(__SIZEOF_FLOAT128__)
	WVRT_PROBE_FLOAT_CONVERSIONS(__float128, signed_value, unsigned_value);
#endif
	wvrt_probe_signed signed_sum, signed_difference;
	wvrt_probe_unsigned unsigned_sum, unsigned_difference;
	int narrowed;
	return (__builtin_add_overflow(quotient, remainder, &signed_sum) ||
		__builtin_sub_overflow(shift, signed_value, &signed_difference) ||
		__builtin_add_overflow(rotated, unsigned_value, &unsigned_sum) ||
		__builtin_sub_overflow(product, unsigned_remainder, &unsigned_difference) ||
		__builtin_add_overflow(0, s, &narrowed) ||
		__builtin_add_overflow(0, narrowed, &unsigned_sum) ||
		__builtin_add_overflow(0, u, &signed_sum) ||
		signed_sum == signed_difference || unsigned_sum == unsigned_difference);
}

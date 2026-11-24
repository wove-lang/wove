// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

// Missing headers still produce a structured probe so Wove can report the import location.
#if __has_include(<@.h>)
#include <@.h>
#define WVPR_HEADER_PRESENT 1
#else
#define WVPR_HEADER_PRESENT 0
#endif

// Detect formats independently of their storage size or spelling on this Clang target.
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
#define WVPR_HAS_FLOAT16 1
#else
#define WVPR_HAS_FLOAT16 0
#endif
#ifdef __SIZEOF_FLOAT128__
#define WVPR_HAS_FLOAT128 1
#else
#define WVPR_HAS_FLOAT128 0
#endif

// Clang's JSON AST carries these constant values back to the Wove type mapper.
enum wvpr_c_probe {
	wvpr_header_present = WVPR_HEADER_PRESENT,
	wvpr_char_signed = ((char)-1 < 0),
	wvpr_short_bits = sizeof(short) * 8,
	wvpr_int_bits = sizeof(int) * 8,
	wvpr_long_bits = sizeof(long) * 8,
	wvpr_long_long_bits = sizeof(long long) * 8,
	wvpr_long_double_mantissa = __LDBL_MANT_DIG__,
	wvpr_has_float16 = WVPR_HAS_FLOAT16,
	wvpr_has_float128 = WVPR_HAS_FLOAT128,
};

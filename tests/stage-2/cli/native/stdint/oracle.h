// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_STDINT_ORACLE_H
#define WOVE_STDINT_ORACLE_H

#include <stdbool.h>
#include <stdint.h>

#define FIXTURE_TYPE(expression, type) \
	_Static_assert(_Generic((expression), type: 1, default: 0), #expression " has the wrong C type")

FIXTURE_TYPE((int8_t)0, signed char);
FIXTURE_TYPE((uint8_t)0, unsigned char);
FIXTURE_TYPE((int16_t)0, short);
FIXTURE_TYPE((uint16_t)0, unsigned short);
FIXTURE_TYPE((int32_t)0, int);
FIXTURE_TYPE((uint32_t)0, unsigned int);
#if defined(__APPLE__)
FIXTURE_TYPE((int64_t)0, long long);
FIXTURE_TYPE((uint64_t)0, unsigned long long);
#elif defined(__GLIBC__)
FIXTURE_TYPE((int64_t)0, long);
FIXTURE_TYPE((uint64_t)0, unsigned long);
#endif

#define FIXTURE_LIMIT(expression, type, value) \
	FIXTURE_TYPE(expression, type); \
	_Static_assert((expression) == (value), #expression " has the wrong value")

FIXTURE_LIMIT(INT8_MIN, int, -128);
FIXTURE_LIMIT(INT8_MAX, int, 127);
FIXTURE_LIMIT(UINT8_MAX, int, 255);
FIXTURE_LIMIT(INT16_MIN, int, -32768);
FIXTURE_LIMIT(INT16_MAX, int, 32767);
FIXTURE_LIMIT(UINT16_MAX, int, 65535);
FIXTURE_LIMIT(INT32_MIN, int, (-2147483647 - 1));
FIXTURE_LIMIT(INT32_MAX, int, 2147483647);
FIXTURE_LIMIT(UINT32_MAX, unsigned int, 4294967295U);
FIXTURE_LIMIT(INT64_MIN, int64_t, (-9223372036854775807LL - 1));
FIXTURE_LIMIT(INT64_MAX, int64_t, 9223372036854775807LL);
FIXTURE_LIMIT(UINT64_MAX, uint64_t, 18446744073709551615ULL);

FIXTURE_TYPE((int_least8_t)0, int8_t);
FIXTURE_TYPE((uint_least8_t)0, uint8_t);
FIXTURE_LIMIT(INT_LEAST8_MIN, int, INT8_MIN);
FIXTURE_LIMIT(INT_LEAST8_MAX, int, INT8_MAX);
FIXTURE_LIMIT(UINT_LEAST8_MAX, int, UINT8_MAX);

FIXTURE_TYPE((int_least16_t)0, int16_t);
FIXTURE_TYPE((uint_least16_t)0, uint16_t);
FIXTURE_LIMIT(INT_LEAST16_MIN, int, INT16_MIN);
FIXTURE_LIMIT(INT_LEAST16_MAX, int, INT16_MAX);
FIXTURE_LIMIT(UINT_LEAST16_MAX, int, UINT16_MAX);

FIXTURE_TYPE((int_least32_t)0, int32_t);
FIXTURE_TYPE((uint_least32_t)0, uint32_t);
FIXTURE_LIMIT(INT_LEAST32_MIN, int, INT32_MIN);
FIXTURE_LIMIT(INT_LEAST32_MAX, int, INT32_MAX);
FIXTURE_LIMIT(UINT_LEAST32_MAX, unsigned int, UINT32_MAX);

FIXTURE_TYPE((int_least64_t)0, int64_t);
FIXTURE_TYPE((uint_least64_t)0, uint64_t);
FIXTURE_LIMIT(INT_LEAST64_MIN, int_least64_t, INT64_MIN);
FIXTURE_LIMIT(INT_LEAST64_MAX, int_least64_t, INT64_MAX);
FIXTURE_LIMIT(UINT_LEAST64_MAX, uint_least64_t, UINT64_MAX);

FIXTURE_TYPE((int_fast8_t)0, int8_t);
FIXTURE_TYPE((uint_fast8_t)0, uint8_t);
FIXTURE_LIMIT(INT_FAST8_MIN, int, INT8_MIN);
FIXTURE_LIMIT(INT_FAST8_MAX, int, INT8_MAX);
FIXTURE_LIMIT(UINT_FAST8_MAX, int, UINT8_MAX);

#if defined(__GLIBC__)
FIXTURE_TYPE((int_fast16_t)0, int64_t);
FIXTURE_TYPE((uint_fast16_t)0, uint64_t);
FIXTURE_LIMIT(INT_FAST16_MIN, int_fast16_t, INT64_MIN);
FIXTURE_LIMIT(INT_FAST16_MAX, int_fast16_t, INT64_MAX);
FIXTURE_LIMIT(UINT_FAST16_MAX, uint_fast16_t, UINT64_MAX);

FIXTURE_TYPE((int_fast32_t)0, int64_t);
FIXTURE_TYPE((uint_fast32_t)0, uint64_t);
FIXTURE_LIMIT(INT_FAST32_MIN, int_fast32_t, INT64_MIN);
FIXTURE_LIMIT(INT_FAST32_MAX, int_fast32_t, INT64_MAX);
FIXTURE_LIMIT(UINT_FAST32_MAX, uint_fast32_t, UINT64_MAX);
#elif defined(__APPLE__)
FIXTURE_TYPE((int_fast16_t)0, int16_t);
FIXTURE_TYPE((uint_fast16_t)0, uint16_t);
FIXTURE_LIMIT(INT_FAST16_MIN, int, INT16_MIN);
FIXTURE_LIMIT(INT_FAST16_MAX, int, INT16_MAX);
FIXTURE_LIMIT(UINT_FAST16_MAX, int, UINT16_MAX);

FIXTURE_TYPE((int_fast32_t)0, int32_t);
FIXTURE_TYPE((uint_fast32_t)0, uint32_t);
FIXTURE_LIMIT(INT_FAST32_MIN, int, INT32_MIN);
FIXTURE_LIMIT(INT_FAST32_MAX, int, INT32_MAX);
FIXTURE_LIMIT(UINT_FAST32_MAX, unsigned int, UINT32_MAX);
#endif

FIXTURE_TYPE((int_fast64_t)0, int64_t);
FIXTURE_TYPE((uint_fast64_t)0, uint64_t);
FIXTURE_LIMIT(INT_FAST64_MIN, int_fast64_t, INT64_MIN);
FIXTURE_LIMIT(INT_FAST64_MAX, int_fast64_t, INT64_MAX);
FIXTURE_LIMIT(UINT_FAST64_MAX, uint_fast64_t, UINT64_MAX);

FIXTURE_TYPE((intptr_t)0, long);
FIXTURE_TYPE((uintptr_t)0, unsigned long);
FIXTURE_LIMIT(INTPTR_MIN, intptr_t, (-9223372036854775807L - 1));
FIXTURE_LIMIT(INTPTR_MAX, intptr_t, 9223372036854775807L);
FIXTURE_LIMIT(UINTPTR_MAX, uintptr_t, 18446744073709551615UL);

FIXTURE_TYPE((intmax_t)0, long);
FIXTURE_TYPE((uintmax_t)0, unsigned long);
FIXTURE_LIMIT(INTMAX_MIN, intmax_t, (-9223372036854775807L - 1));
FIXTURE_LIMIT(INTMAX_MAX, intmax_t, 9223372036854775807L);
FIXTURE_LIMIT(UINTMAX_MAX, uintmax_t, 18446744073709551615UL);

FIXTURE_LIMIT(PTRDIFF_MIN, long, (-9223372036854775807L - 1));
FIXTURE_LIMIT(PTRDIFF_MAX, long, 9223372036854775807L);
FIXTURE_LIMIT(SIZE_MAX, unsigned long, 18446744073709551615UL);
FIXTURE_LIMIT(SIG_ATOMIC_MIN, int, (-2147483647 - 1));
FIXTURE_LIMIT(SIG_ATOMIC_MAX, int, 2147483647);

// The glibc arm64 wide character limits have unsigned int type.
#if defined(__linux__) && defined(__aarch64__)
typedef unsigned int fixture_wchar_limit_t;
FIXTURE_LIMIT(WCHAR_MIN, fixture_wchar_limit_t, 0U);
FIXTURE_LIMIT(WCHAR_MAX, fixture_wchar_limit_t, 4294967295U);
#else
typedef int fixture_wchar_limit_t;
FIXTURE_LIMIT(WCHAR_MIN, fixture_wchar_limit_t, (-2147483647 - 1));
FIXTURE_LIMIT(WCHAR_MAX, fixture_wchar_limit_t, 2147483647);
#endif

// The glibc wide integer limits have unsigned int type on both targets.
#if defined(__linux__)
typedef unsigned int fixture_wint_limit_t;
FIXTURE_LIMIT(WINT_MIN, fixture_wint_limit_t, 0U);
FIXTURE_LIMIT(WINT_MAX, fixture_wint_limit_t, 4294967295U);
#else
typedef int fixture_wint_limit_t;
FIXTURE_LIMIT(WINT_MIN, fixture_wint_limit_t, (-2147483647 - 1));
FIXTURE_LIMIT(WINT_MAX, fixture_wint_limit_t, 2147483647);
#endif

#undef FIXTURE_LIMIT
#undef FIXTURE_TYPE

// Check both signed endpoints and the unsigned maximum passed through the native typedefs.
static inline bool fixture_bounds8(int8_t minimum, int8_t maximum, uint8_t unsigned_maximum) {
	return minimum == INT8_MIN && maximum == INT8_MAX && unsigned_maximum == UINT8_MAX;
}

// Check both signed endpoints and the unsigned maximum passed through the native typedefs.
static inline bool fixture_bounds16(int16_t minimum, int16_t maximum, uint16_t unsigned_maximum) {
	return minimum == INT16_MIN && maximum == INT16_MAX && unsigned_maximum == UINT16_MAX;
}

// Check both signed endpoints and the unsigned maximum passed through the native typedefs.
static inline bool fixture_bounds32(int32_t minimum, int32_t maximum, uint32_t unsigned_maximum) {
	return minimum == INT32_MIN && maximum == INT32_MAX && unsigned_maximum == UINT32_MAX;
}

// Check both signed endpoints and the unsigned maximum passed through the native typedefs.
static inline bool fixture_bounds64(int64_t minimum, int64_t maximum, uint64_t unsigned_maximum) {
	return minimum == INT64_MIN && maximum == INT64_MAX && unsigned_maximum == UINT64_MAX;
}

// Verify all three least-8 endpoints after passing through the native types.
static inline bool fixture_least8(int_least8_t minimum, int_least8_t maximum, uint_least8_t unsigned_maximum) {
	return minimum == INT_LEAST8_MIN && maximum == INT_LEAST8_MAX && unsigned_maximum == UINT_LEAST8_MAX;
}

// Verify all three least-16 endpoints after passing through the native types.
static inline bool fixture_least16(int_least16_t minimum, int_least16_t maximum, uint_least16_t unsigned_maximum) {
	return minimum == INT_LEAST16_MIN && maximum == INT_LEAST16_MAX && unsigned_maximum == UINT_LEAST16_MAX;
}

// Verify all three least-32 endpoints after passing through the native types.
static inline bool fixture_least32(int_least32_t minimum, int_least32_t maximum, uint_least32_t unsigned_maximum) {
	return minimum == INT_LEAST32_MIN && maximum == INT_LEAST32_MAX && unsigned_maximum == UINT_LEAST32_MAX;
}

// Verify all three least-64 endpoints after passing through the native types.
static inline bool fixture_least64(int_least64_t minimum, int_least64_t maximum, uint_least64_t unsigned_maximum) {
	return minimum == INT_LEAST64_MIN && maximum == INT_LEAST64_MAX && unsigned_maximum == UINT_LEAST64_MAX;
}

// Verify all three fast-8 endpoints after passing through the native types.
static inline bool fixture_fast8(int_fast8_t minimum, int_fast8_t maximum, uint_fast8_t unsigned_maximum) {
	return minimum == INT_FAST8_MIN && maximum == INT_FAST8_MAX && unsigned_maximum == UINT_FAST8_MAX;
}

// Verify all three fast-16 endpoints after passing through the native types.
static inline bool fixture_fast16(int_fast16_t minimum, int_fast16_t maximum, uint_fast16_t unsigned_maximum) {
	return minimum == INT_FAST16_MIN && maximum == INT_FAST16_MAX && unsigned_maximum == UINT_FAST16_MAX;
}

// Verify all three fast-32 endpoints after passing through the native types.
static inline bool fixture_fast32(int_fast32_t minimum, int_fast32_t maximum, uint_fast32_t unsigned_maximum) {
	return minimum == INT_FAST32_MIN && maximum == INT_FAST32_MAX && unsigned_maximum == UINT_FAST32_MAX;
}

// Verify all three fast-64 endpoints after passing through the native types.
static inline bool fixture_fast64(int_fast64_t minimum, int_fast64_t maximum, uint_fast64_t unsigned_maximum) {
	return minimum == INT_FAST64_MIN && maximum == INT_FAST64_MAX && unsigned_maximum == UINT_FAST64_MAX;
}

// Compare the pointer-width endpoints passed through the native pointer-width typedefs.
static inline bool fixture_pointer_limits(intptr_t minimum, intptr_t maximum, uintptr_t unsigned_maximum) {
	return minimum == INTPTR_MIN && maximum == INTPTR_MAX && unsigned_maximum == UINTPTR_MAX;
}

// Compare the maximum-width endpoints passed through the native maximum-width typedefs.
static inline bool fixture_maximum_limits(intmax_t minimum, intmax_t maximum, uintmax_t unsigned_maximum) {
	return minimum == INTMAX_MIN && maximum == INTMAX_MAX && unsigned_maximum == UINTMAX_MAX;
}

// Compare the endpoints received with the native long, unsigned long and int argument types.
static inline bool fixture_traditional_limits(long pointer_minimum, long pointer_maximum,
		unsigned long size_maximum, int signal_minimum, int signal_maximum) {
	return pointer_minimum == PTRDIFF_MIN && pointer_maximum == PTRDIFF_MAX &&
		size_maximum == SIZE_MAX && signal_minimum == SIG_ATOMIC_MIN && signal_maximum == SIG_ATOMIC_MAX;
}

// Compare the wide character endpoints through the target's signed or unsigned C type.
static inline bool fixture_wchar_limits(fixture_wchar_limit_t minimum, fixture_wchar_limit_t maximum) {
	return minimum == WCHAR_MIN && maximum == WCHAR_MAX;
}

// Compare the wide integer endpoints through the target's signed or unsigned C type.
static inline bool fixture_wint_limits(fixture_wint_limit_t minimum, fixture_wint_limit_t maximum) {
	return minimum == WINT_MIN && maximum == WINT_MAX;
}

#endif

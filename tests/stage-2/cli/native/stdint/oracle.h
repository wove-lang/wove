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

#endif

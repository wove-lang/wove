// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_INTTYPES_ORACLE_H
#define WOVE_TEST_INTTYPES_ORACLE_H

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

_Static_assert(_Generic(&imaxabs, intmax_t (*)(intmax_t): 1, default: 0), "imaxabs type");
_Static_assert(_Generic(&imaxdiv, imaxdiv_t (*)(intmax_t, intmax_t): 1, default: 0), "imaxdiv type");
_Static_assert(_Generic(((imaxdiv_t *)0)->quot, intmax_t: 1, default: 0), "quot type");
_Static_assert(_Generic(((imaxdiv_t *)0)->rem, intmax_t: 1, default: 0), "rem type");
_Static_assert(sizeof(imaxdiv_t) == 2 * sizeof(intmax_t), "division size");
_Static_assert(_Alignof(imaxdiv_t) == _Alignof(intmax_t), "division alignment");
_Static_assert(offsetof(imaxdiv_t, quot) == 0, "quot offset");
_Static_assert(offsetof(imaxdiv_t, rem) == sizeof(intmax_t), "rem offset");

// Check the record received from Wove against the actual C by-value result.
static inline bool fixture_division_fields(const imaxdiv_t *observed)
{
	imaxdiv_t native = imaxdiv(-43, 7);
	return observed->quot == native.quot && observed->rem == native.rem;
}

// Compare the active C dialect's treatment of an auto-base binary prefix.
static inline bool fixture_binary_prefix(intmax_t signed_value, int32_t signed_end,
	uintmax_t unsigned_value, int32_t unsigned_end)
{
	const char text[] = "0b101!";
	char *signed_stop;
	char *unsigned_stop;
	const intmax_t native_signed = strtoimax(text, &signed_stop, 0);
	const uintmax_t native_unsigned = strtoumax(text, &unsigned_stop, 0);
	return signed_value == native_signed && signed_end == signed_stop - text &&
		unsigned_value == native_unsigned && unsigned_end == unsigned_stop - text;
}

#endif

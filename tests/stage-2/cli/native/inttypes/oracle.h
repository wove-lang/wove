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

#endif

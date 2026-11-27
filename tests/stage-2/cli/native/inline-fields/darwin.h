/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_DARWIN_INLINE_FIELDS_H
#define WOVE_TEST_DARWIN_INLINE_FIELDS_H

#include <fcntl.h>
#include <stddef.h>

_Static_assert(sizeof(fattributiontag_t) == 272, "Darwin attribution record size");
_Static_assert(_Alignof(fattributiontag_t) == 8, "Darwin attribution record alignment");
_Static_assert(offsetof(fattributiontag_t, ft_flags) == 0 &&
	offsetof(fattributiontag_t, ft_hash) == 8 &&
	offsetof(fattributiontag_t, ft_attribution_name) == 16, "Darwin attribution field offsets");

#endif

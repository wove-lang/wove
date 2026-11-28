/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FCNTL_ATTRIBUTION_H
#define WOVE_TEST_FCNTL_ATTRIBUTION_H

#include <fcntl.h>
#include <stddef.h>

_Static_assert(sizeof(fattributiontag_t) == 272 && _Alignof(fattributiontag_t) == 8,
	"native attribution record layout");
_Static_assert(offsetof(fattributiontag_t, ft_flags) == 0 &&
	offsetof(fattributiontag_t, ft_hash) == 8 &&
	offsetof(fattributiontag_t, ft_attribution_name) == 16,
	"native attribution field offsets");

/* Call the platform query with the control record so the fixture can compare both results. */
static inline int native_attribution_query(int fd, fattributiontag_t *tag)
{
	return fcntl(fd, F_ATTRIBUTION_TAG, tag);
}

#endif

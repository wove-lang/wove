/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FILESEC_PROPERTIES_H
#define WOVE_TEST_FILESEC_PROPERTIES_H

#include <fcntl.h>

_Static_assert(_Generic(&filesec_query_property,
	int (*)(filesec_t, filesec_property_t, int *): 1, default: 0), "filesec_query_property prototype");
_Static_assert(_Generic(&filesec_unset_property,
	int (*)(filesec_t, filesec_property_t): 1, default: 0), "filesec_unset_property prototype");

/* Prepare a present scalar property through the native setter. */
static inline int seed_mode(filesec_t security, mode_t mode)
{
	return filesec_set_property(security, FILESEC_MODE, &mode);
}

#endif

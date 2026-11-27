/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FILESEC_UUID_H
#define WOVE_TEST_FILESEC_UUID_H

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <uuid/uuid.h>

_Static_assert(sizeof(uuid_t) == 16, "filesec UUID fixtures require the native UUID width");

/* Set an independent native UUID pattern using the C library directly. */
static inline int seed_uuid(filesec_t security, filesec_property_t property, uint8_t first)
{
	uuid_t value;
	for (size_t i = 0; i < sizeof(value); ++i) {
		value[i] = (uint8_t)(first + i);
	}
	return filesec_set_property(security, property, value);
}

/* Read a property directly and compare every byte with the expected pattern. */
static inline int verify_uuid(filesec_t security, filesec_property_t property, uint8_t first)
{
	uuid_t value;
	int status = filesec_get_property(security, property, value);
	if (status != 0) {
		return status;
	}
	for (size_t i = 0; i < sizeof(value); ++i) {
		if (value[i] != (uint8_t)(first + i)) {
			return 1;
		}
	}
	return 0;
}

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_PROFILE_SNAPSHOT_H
#define WOVE_TEST_PROFILE_SNAPSHOT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
#if defined(HIDDEN_SNAPSHOT_EXTENT)
	uint32_t reserved_extent;
#elif defined(NARROW_SNAPSHOT_EXTENT)
	uint16_t extent;
#else
	uint32_t extent;
#endif
	char text[4];
} ProfileSnapshot;

/* Supply the complete record without selecting its members in Wove. */
static inline void fixture_seed_snapshot(ProfileSnapshot *value)
{
#ifndef HIDDEN_SNAPSHOT_EXTENT
	value->extent = offsetof(ProfileSnapshot, text) + sizeof("A");
#endif
	value->text[0] = 'A';
	value->text[1] = '\0';
}

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_INLINE_FIELDS_H
#define WOVE_TEST_INLINE_FIELDS_H

#include <stddef.h>
#include <stdint.h>

#ifndef WOVE_TAG_NAME
#define WOVE_TAG_NAME char name[17]
#endif

typedef struct {
	uint32_t before;
	WOVE_TAG_NAME;
	uint32_t after;
} TestTag;

_Static_assert(offsetof(TestTag, name) == 4, "the name follows the first guard");

/* Check the original object's input bytes and return new high bytes through the same member. */
static inline int exchange_tag(TestTag *tag)
{
	unsigned char *name = (unsigned char *)tag->name;
	if (tag->before != UINT32_C(0x12345678) || tag->after != UINT32_C(0xcafefeed) ||
		name[0] != 0xff || name[8] != 0 || name[9] != 0xf1 || name[16] != 0x80) {
		return -1;
	}
	name[0] = 0xe3;
	name[9] = 0;
	name[16] = 0xa5;
	return 0;
}

#endif

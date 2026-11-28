/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_MACRO_MEMBER_H
#define WOVE_TEST_MACRO_MEMBER_H

#include <stddef.h>
#include <stdint.h>

struct fixture_row {
	uint8_t first;
	uint64_t value;
};

typedef struct {
	uint32_t flags : 3;
} FixtureBits;

#if defined(BAD_RESULT)
#define FIXTURE_OFFSET(type, member) ((double)__builtin_offsetof(type, member))
#else
#define FIXTURE_OFFSET(type, member) __builtin_offsetof(type, member)
#endif

/* Compare both Wove results against the native ABI of this header. */
static inline int fixture_check(size_t first, size_t value)
{
	return first == offsetof(struct fixture_row, first) &&
	    value == offsetof(struct fixture_row, value) ? 42 : 1;
}

#endif

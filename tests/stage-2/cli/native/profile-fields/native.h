/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_PROFILE_FIELDS_H
#define WOVE_TEST_PROFILE_FIELDS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	uint32_t prefix;
#ifdef PROFILE_FIELDS
#ifdef WRONG_COUNTER
	int64_t counter;
#else
	int32_t counter;
#endif
#ifdef WRONG_EXTENT
	char label[9];
#else
	char label[8];
#endif
#else
	int32_t reserved_counter;
	char reserved_label[8];
#endif
#ifdef WRONG_TAIL
	int64_t tail;
#else
	uint64_t tail;
#endif
} ProfileRecord;

/* Seed storage on both sides of the profile-dependent fields. */
static inline void fixture_seed(ProfileRecord *value)
{
	value->prefix = 42;
	value->tail = UINT64_C(0x123456789abcdef0);
}

/* Compare the Wove allocation's C type with the selected header's full record. */
#define fixture_layout(value) \
	(sizeof(*(value)) == sizeof(ProfileRecord) && \
	    _Alignof(__typeof__(*(value))) == _Alignof(ProfileRecord) && \
	    (uintptr_t)(value) % _Alignof(ProfileRecord) == 0)

/* Compare with C's actual designator and offset when the profile exposes it. */
static inline int32_t fixture_offset(void)
{
#ifdef PROFILE_FIELDS
	return (int32_t)offsetof(ProfileRecord, label);
#else
	return -1;
#endif
}
#endif

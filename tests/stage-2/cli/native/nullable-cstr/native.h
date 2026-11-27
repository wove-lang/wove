/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_NULLABLE_CSTR_H
#define WOVE_TEST_NULLABLE_CSTR_H

#include <stddef.h>
#include <string.h>

static int fixture_step;

/* Return the next evaluation index, or fail when arguments evaluate out of order. */
static inline int fixture_next(int expected)
{
	return ++fixture_step == expected ? expected : -1;
}

/* Check null, empty, and nonempty strings at exact positions in the native prototype. */
static inline int fixture_nullable(const char *first, int stamp, const char *fixed,
	const char *second, const char *third, int tail)
{
	if (fixed != NULL || fixture_step != 3)
		return 1;
	if (stamp == 1 && tail == 3 && first == NULL && second != NULL && second[0] == '\0'
		&& third != NULL && strcmp(third, "value") == 0)
		return 42;
	if (stamp == 4 && tail == 5 && first == NULL && second == NULL && third == NULL)
		return 42;
	if (stamp == 6 && tail == 7 && first != NULL && strcmp(first, "value") == 0
		&& second == NULL && third != NULL && third[0] == '\0')
		return 42;
	return 2;
}

/* Verify the mutable string retains its compact descriptor index for copy-back. */
static inline int fixture_mutate(const char *omitted, char *value, const char *suffix)
{
	if (omitted != NULL || value == NULL || suffix == NULL
		|| strcmp(value, "ab") != 0 || strcmp(suffix, "value") != 0)
		return 1;
	value[0] = 'z';
	return 42;
}

/* The trap fixture reports an unexpected native entry through the return status. */
static inline int fixture_single(const char *value)
{
	return value == NULL ? 98 : 99;
}

#endif

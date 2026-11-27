/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_VOID_POINTER_VIEWS_H
#define WOVE_TEST_VOID_POINTER_VIEWS_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uintptr_t expected_input;
static uintptr_t expected_output;
static unsigned char recorded[16];
static int32_t step;

/* Capture addresses as integers so the native fixture never retains a live loan. */
static inline void remember_views(const void *input, void *output, int32_t count)
{
	if (count != (int32_t)sizeof(recorded)) {
		abort();
	}
	expected_input = (uintptr_t)input;
	expected_output = (uintptr_t)output;
	step = 0;
}

/* Return a range endpoint or token after proving its evaluation order. */
static inline int32_t view_step(int32_t expected, int32_t value)
{
	if (step != expected) {
		abort();
	}
	++step;
	return value;
}

/* Copy bytes from the exact shared view at its original address. */
static inline int32_t set_view(int32_t selector, const void *input, int32_t token)
{
	if ((uintptr_t)input != expected_input || selector != 42 || token != 17) {
		abort();
	}
	memcpy(recorded, input, sizeof(recorded));
	return 0;
}

/* Write the saved bytes to the exact mutable view at its original address. */
static inline int32_t get_view(void *output, int32_t token)
{
	if ((uintptr_t)output != expected_output || token != 19) {
		abort();
	}
	memcpy(output, recorded, sizeof(recorded));
	return 0;
}

/* Declare the two access modes on one call for the overlapping-loan fixture. */
static inline void loan_pair(const void *input, void *output)
{
	memcpy(output, input, sizeof(recorded));
}

/* Give a shared Wove formal the wrong native pointee qualifier. */
static inline void wrong_shared(void *input)
{
	(void)input;
}

/* Give a mutable Wove formal the wrong native pointee qualifier. */
static inline void wrong_mutable(const void *output)
{
	(void)output;
}

#endif

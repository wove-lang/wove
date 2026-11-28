// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_STDDEF_ORACLE_H
#define WOVE_TEST_STDDEF_ORACLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Return the upper limit of the actual unsigned native type.
static inline size_t fixture_size(void)
{
	return SIZE_MAX;
}

// Return the lower limit of the actual signed native type.
static inline ptrdiff_t fixture_difference(void)
{
	return PTRDIFF_MIN;
}

// Exercise the largest value for the target's signed or unsigned wchar_t.
static inline wchar_t fixture_wide(void)
{
	return WCHAR_MAX;
}

// Compare the three Wove conversions to the same native limits.
static inline bool fixture_matches(size_t size, ptrdiff_t difference, wchar_t wide)
{
	return size == SIZE_MAX && difference == PTRDIFF_MIN && wide == WCHAR_MAX;
}

#if defined(__STDC_WANT_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__ == 1
// Return the maximum value representable by the Annex K typedef.
static inline rsize_t fixture_rsize(void)
{
	return (rsize_t)-1;
}

// Check that the Wove alias preserves the native value.
static inline bool fixture_check_rsize(rsize_t value)
{
	return value == (rsize_t)-1;
}
#endif

#if defined(__linux__)
// Read the address with the native type and test its required alignment.
static inline bool fixture_check_alignment(const max_align_t *value)
{
	return (uintptr_t)value % _Alignof(max_align_t) == 0;
}
#endif
#endif

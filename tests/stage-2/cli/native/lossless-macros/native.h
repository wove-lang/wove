/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_LOSSLESS_MACROS_H
#define WOVE_TEST_LOSSLESS_MACROS_H

#include <stdint.h>

#if defined(WOVE_DYNAMIC_MACRO)
static int32_t wove_macro_reads;
static inline long wove_next_sample(void)
{
	return 40L + ++wove_macro_reads;
}
#define WOVE_SAMPLE (wove_next_sample())
#elif defined(WOVE_FLOAT_MACRO)
#define WOVE_SAMPLE 42.0
#elif defined(WOVE_POINTER_MACRO)
#define WOVE_SAMPLE ((void *)0)
#elif defined(WOVE_UNSIGNED_MACRO)
#define WOVE_SAMPLE 42ULL
#elif defined(WOVE_FUNCTION_MACRO)
enum { WOVE_SAMPLE = 42 };
#define WOVE_SAMPLE(value) (value)
#elif !defined(WOVE_MISSING_MACRO)
#define WOVE_SAMPLE 42
#endif

#define WOVE_EXACT WOVE_SAMPLE
#define WOVE_CONSTANT WOVE_SAMPLE

#ifdef WOVE_NARROWING_MACRO
#define WOVE_BYTE ((unsigned short)42)
#else
#define WOVE_BYTE ((char)42)
#endif

static inline int32_t wove_ordered_samples(int64_t first, int64_t second)
{
#ifdef WOVE_DYNAMIC_MACRO
	return first == 41 && second == 42 && wove_macro_reads == 2 ? 42 : 1;
#else
	return first == 42 && second == 42 ? 42 : 1;
#endif
}

#endif

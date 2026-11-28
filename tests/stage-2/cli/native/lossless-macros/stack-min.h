/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_GLIBC_STACK_MIN_H
#define WOVE_TEST_GLIBC_STACK_MIN_H

#include <limits.h>
#include <stdint.h>

#if defined(_GNU_SOURCE)
_Static_assert(_Generic((PTHREAD_STACK_MIN), long: 1, default: 0),
	"the GNU profile exposes a long expression");
#else
_Static_assert(_Generic((PTHREAD_STACK_MIN), int: 1, default: 0),
	"the POSIX profile exposes an int");
enum { wove_posix_stack_min = PTHREAD_STACK_MIN };
#endif

static inline int64_t wove_stack_min_c(void)
{
	return (int64_t)PTHREAD_STACK_MIN;
}

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TIMEZONE_ORACLE_H
#define WOVE_TEST_TIMEZONE_ORACLE_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/time.h>

_Static_assert(_Generic(&gettimeofday, int (*)(struct timeval *, void *): 1, default: 0),
	"gettimeofday accepts a native void pointer");

static const struct timezone initial_zone = {.tz_minuteswest = 717, .tz_dsttime = 19};

// Check one direct native call with the same initial storage as the Wove call.
// Both reads remain valid on libc implementations that leave the legacy fields unchanged.
static inline bool timezone_matches(const struct timeval *sample, const struct timezone *zone,
	int32_t west, int32_t dst, int32_t status)
{
	struct timeval comparison = {0};
	struct timezone expected = initial_zone;
	int32_t compare_status = gettimeofday(&comparison, &expected);
	return status == 0 && status == compare_status && comparison.tv_sec > 0
		&& sample->tv_sec > 0 && sample->tv_usec >= 0 && sample->tv_usec < 1000000
		&& zone->tz_minuteswest == west && zone->tz_dsttime == dst
		&& west == expected.tz_minuteswest && dst == expected.tz_dsttime;
}

// Check that the fixed-null variant still reads a fresh wall-clock value.
static inline bool timezone_null_sample(const struct timeval *sample)
{
	return sample->tv_sec > 0 && sample->tv_usec >= 0 && sample->tv_usec < 1000000;
}

#endif

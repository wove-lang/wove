// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TIMES_ORACLE_H
#define WOVE_TEST_TIMES_ORACLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/times.h>

#if defined(__APPLE__)
_Static_assert(_Generic((clock_t)0, unsigned long: 1, default: 0), "Darwin clock_t");
#elif defined(__linux__)
_Static_assert(_Generic((clock_t)0, long: 1, default: 0), "glibc clock_t");
#endif

_Static_assert(sizeof(clock_t) == 8 && _Alignof(clock_t) == 8, "clock_t layout");
_Static_assert(sizeof(struct tms) == 32 && _Alignof(struct tms) == 8, "tms layout");
_Static_assert(offsetof(struct tms, tms_utime) == 0 && offsetof(struct tms, tms_stime) == 8
	&& offsetof(struct tms, tms_cutime) == 16 && offsetof(struct tms, tms_cstime) == 24, "tms offsets");
_Static_assert(_Generic(((struct tms *)0)->tms_utime, clock_t: 1, default: 0)
	&& _Generic(((struct tms *)0)->tms_stime, clock_t: 1, default: 0)
	&& _Generic(((struct tms *)0)->tms_cutime, clock_t: 1, default: 0)
	&& _Generic(((struct tms *)0)->tms_cstime, clock_t: 1, default: 0), "tms member types");
_Static_assert(_Generic(&times, clock_t (*)(struct tms *): 1, default: 0), "times prototype");

// Supply four distinct native values that exceed 32 bits.
static inline void fixture_seed_times(struct tms *sample)
{
	*sample = (struct tms){
		.tms_utime = (clock_t)UINT64_C(4294967307),
		.tms_stime = (clock_t)UINT64_C(4294967309),
		.tms_cutime = (clock_t)UINT64_C(4294967313),
		.tms_cstime = (clock_t)UINT64_C(4294967315),
	};
}

// Return the native error value with the selected clock_t type.
static inline clock_t fixture_clock_error(void)
{
	return (clock_t)-1;
}

// Recognize the native sentinel after it has crossed the bridge.
static inline bool fixture_is_clock_error(clock_t value)
{
	return value == (clock_t)-1;
}

// Bracket the Wove call with direct C calls to the same header function.
static inline clock_t fixture_read_ticks(void)
{
	struct tms sample;
	return times(&sample);
}

// Check the raw result in the native type and reject failed reference calls.
static inline bool fixture_ticks_between(clock_t before, clock_t value, clock_t after)
{
	return before != (clock_t)-1 && after != (clock_t)-1 && before <= value && value <= after;
}

// Every value must match its native field and replace the original sentinel.
static inline bool fixture_match_times(const struct tms *sample, clock_t user, clock_t system,
	clock_t child_user, clock_t child_system)
{
	return user != (clock_t)-1 && user == sample->tms_utime
		&& system != (clock_t)-1 && system == sample->tms_stime
		&& child_user != (clock_t)-1 && child_user == sample->tms_cutime
		&& child_system != (clock_t)-1 && child_system == sample->tms_cstime;
}

#endif

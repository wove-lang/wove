// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TIMEVAL_ORACLE_H
#define WOVE_TEST_TIMEVAL_ORACLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

#if defined(__APPLE__)
typedef int FixtureTimerWhich;
_Static_assert(_Generic((suseconds_t)0, int: 1, default: 0), "Darwin microseconds");
#else
typedef __itimer_which_t FixtureTimerWhich;
_Static_assert(_Generic((suseconds_t)0, long: 1, default: 0), "glibc microseconds");
#endif

_Static_assert(sizeof(struct timeval) == 16 && _Alignof(struct timeval) == 8, "timeval layout");
_Static_assert(offsetof(struct timeval, tv_sec) == 0 && offsetof(struct timeval, tv_usec) == 8,
	"timeval offsets");
_Static_assert(_Generic(((struct timeval *)0)->tv_sec, time_t: 1, default: 0)
	&& _Generic(((struct timeval *)0)->tv_usec, suseconds_t: 1, default: 0), "timeval types");
_Static_assert(_Generic(&gettimeofday, int (*)(struct timeval *, void *): 1, default: 0),
	"gettimeofday prototype");
_Static_assert(sizeof(struct itimerval) == 32 && _Alignof(struct itimerval) == 8, "itimerval layout");
_Static_assert(offsetof(struct itimerval, it_interval) == 0 && offsetof(struct itimerval, it_value) == 16,
	"itimerval offsets");
_Static_assert(_Generic(&getitimer, int (*)(FixtureTimerWhich, struct itimerval *): 1, default: 0),
	"getitimer prototype");
_Static_assert(_Generic(&setitimer, int (*)(FixtureTimerWhich, const struct itimerval *, struct itimerval *):
	1, default: 0), "setitimer prototype");

// Supply a distinctive full-width value independently of the host clock.
static inline void fixture_seed_timeval(struct timeval *sample)
{
	*sample = (struct timeval){.tv_sec = (time_t)INT64_C(4294967307), .tv_usec = 531234};
}

// Read wall time directly from the same SDK.
static inline int32_t fixture_wall_status(void)
{
	struct timeval sample;
	return gettimeofday(&sample, NULL);
}

// Compare the Wove projections to the original live C object.
static inline bool fixture_wall_time(const struct timeval *sample, time_t seconds, suseconds_t micros)
{
	return seconds == sample->tv_sec && micros == sample->tv_usec && seconds > 0 && micros >= 0
		&& micros < 1000000;
}

// Compare all three Wove selectors against the actual header.
static inline bool fixture_timer_selectors(FixtureTimerWhich real, FixtureTimerWhich virtual_time,
	FixtureTimerWhich profiling)
{
	return real == ITIMER_REAL && virtual_time == ITIMER_VIRTUAL && profiling == ITIMER_PROF;
}

// Check the C status using an independent output record.
static inline int32_t fixture_timer_status(FixtureTimerWhich which)
{
	struct itimerval sample;
	return getitimer(which, &sample);
}

// Check all four Wove projections against the two native time values.
static inline bool fixture_timer_fields(const struct itimerval *sample, time_t interval_sec,
	suseconds_t interval_usec, time_t value_sec, suseconds_t value_usec)
{
	return interval_sec == sample->it_interval.tv_sec && interval_usec == sample->it_interval.tv_usec
		&& value_sec == sample->it_value.tv_sec && value_usec == sample->it_value.tv_usec
		&& interval_sec >= 0 && interval_usec >= 0 && interval_usec < 1000000
		&& value_sec >= 0 && value_usec >= 0 && value_usec < 1000000;
}

// The fresh test process and its saved previous value should have disarmed timers.
static inline bool fixture_timer_disarmed(const struct itimerval *sample)
{
	return sample->it_interval.tv_sec == 0 && sample->it_interval.tv_usec == 0
		&& sample->it_value.tv_sec == 0 && sample->it_value.tv_usec == 0;
}

// The first expiration is later than the fixture timeout.
static inline bool fixture_timer_armed(const struct itimerval *sample)
{
	return sample->it_interval.tv_sec == 37 && sample->it_interval.tv_usec == 530000
		&& sample->it_value.tv_sec >= 90 && sample->it_value.tv_sec <= 120;
}

#endif

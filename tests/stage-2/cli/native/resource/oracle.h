// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_RESOURCE_ORACLE_H
#define WOVE_TEST_RESOURCE_ORACLE_H

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/resource.h>
#include <unistd.h>

#if defined(__APPLE__)
typedef int FixtureWho;
typedef int FixturePriorityWhich;
typedef int FixtureMicros;
#else
typedef __rusage_who_t FixtureWho;
typedef __priority_which_t FixturePriorityWhich;
typedef long FixtureMicros;
#endif

_Static_assert(sizeof(struct rusage) == 144 && _Alignof(struct rusage) == 8, "rusage layout");
_Static_assert(sizeof(struct timeval) == 16 && _Alignof(struct timeval) == 8, "timeval layout");
_Static_assert(offsetof(struct rusage, ru_utime) == 0 && offsetof(struct rusage, ru_stime) == 16,
	"rusage time offsets");
_Static_assert(offsetof(struct timeval, tv_sec) == 0 && offsetof(struct timeval, tv_usec) == 8,
	"timeval member offsets");
_Static_assert(_Generic(((struct timeval *)0)->tv_sec, long: 1, default: 0)
	&& _Generic(((struct timeval *)0)->tv_usec, FixtureMicros: 1, default: 0), "timeval field types");
_Static_assert(_Generic(&getrusage, int (*)(FixtureWho, struct rusage *): 1, default: 0), "getrusage prototype");

// Seed distinct seconds and subsecond values through the native field names.
static inline void fixture_seed_usage(struct rusage *usage)
{
	*usage = (struct rusage){
		.ru_utime = {.tv_sec = 4294967307L, .tv_usec = 123456},
		.ru_stime = {.tv_sec = 4294967309L, .tv_usec = 654321},
	};
}

// Compare each Wove selector with the native value in the actual header.
static inline bool fixture_usage_selectors(FixtureWho self, FixtureWho children)
{
	return self == RUSAGE_SELF && children == RUSAGE_CHILDREN;
}

// Call the same header directly with a separate native record.
static inline int32_t fixture_usage_status(FixtureWho who)
{
	struct rusage native;
	return getrusage(who, &native);
}

// Confirm the native call replaced the seeded times and Wove read the correct fields.
static inline bool fixture_match_usage(const struct rusage *usage, long user_seconds, FixtureMicros user_micros,
	long system_seconds, FixtureMicros system_micros)
{
	return user_seconds != 4294967307L && system_seconds != 4294967309L
		&& user_seconds == usage->ru_utime.tv_sec && user_micros == usage->ru_utime.tv_usec
		&& system_seconds == usage->ru_stime.tv_sec && system_micros == usage->ru_stime.tv_usec
		&& user_seconds >= 0 && user_micros >= 0 && user_micros < 1000000
		&& system_seconds >= 0 && system_micros >= 0 && system_micros < 1000000;
}

#ifdef TEST_MEMBER
// Read the selected GNU thread-accounting constant from the live header.
static inline FixtureWho fixture_optional_usage_scope(void)
{
	return TEST_MEMBER;
}
#endif


/* Seed distinct signed-long counters when the native header exposes them. */
static inline void fixture_seed_counters(struct rusage *value)
{
#if defined(__linux__) || (defined(__APPLE__) && __DARWIN_C_LEVEL >= __DARWIN_C_FULL)
	value->ru_maxrss = LONG_MAX - 0;
	value->ru_ixrss = LONG_MAX - 1;
	value->ru_idrss = LONG_MAX - 2;
	value->ru_isrss = LONG_MAX - 3;
	value->ru_minflt = LONG_MAX - 4;
	value->ru_majflt = LONG_MAX - 5;
	value->ru_nswap = LONG_MAX - 6;
	value->ru_inblock = LONG_MAX - 7;
	value->ru_oublock = LONG_MAX - 8;
	value->ru_msgsnd = LONG_MAX - 9;
	value->ru_msgrcv = LONG_MAX - 10;
	value->ru_nsignals = LONG_MAX - 11;
	value->ru_nvcsw = LONG_MAX - 12;
	value->ru_nivcsw = LONG_MAX - 13;
#else
	(void)value;
#endif
}

/* Use the exact native value as the oracle for each Wove field. */
static inline bool fixture_counter_value(int32_t index, long value)
{
	return value == LONG_MAX - index;
}

_Static_assert(_Generic((id_t)0, uint32_t: 1, default: 0), "priority identifier type");
_Static_assert(_Generic(&getpriority, int (*)(FixturePriorityWhich, id_t): 1, default: 0),
	"getpriority prototype");
_Static_assert(_Generic(&setpriority, int (*)(FixturePriorityWhich, id_t, int): 1, default: 0),
	"setpriority prototype");

#ifdef TEST_MEMBER
/* Compare the Wove constant with the native expression selected by the fixture. */
static inline int32_t fixture_priority_constant(void)
{
	return (int32_t)TEST_MEMBER;
}
#endif

/* Compare the three portable selectors against definitions from the live header. */
static inline bool fixture_priority_selectors(FixturePriorityWhich process, FixturePriorityWhich group,
	FixturePriorityWhich user)
{
	return process == PRIO_PROCESS && group == PRIO_PGRP && user == PRIO_USER;
}

/* Identify only the fixture subprocess for the priority change. */
static inline id_t fixture_priority_pid(void)
{
	return (id_t)getpid();
}

/* Check the priority as read independently by the native function. */
static inline bool fixture_priority_matches(id_t id, int32_t expected)
{
	errno = 0;
	const int actual = getpriority(PRIO_PROCESS, id);
	return errno == 0 && actual == expected;
}

#ifdef WOVE_TEST_PRIORITY_SENTINEL
/* Produce both meanings of -1 through the same exact native prototype. */
static inline int fixture_priority_sentinel(FixturePriorityWhich which, id_t id)
{
	/* Keep errno intact on the simulated success path. */
	if (which != PRIO_PROCESS || id != 0) {
		errno = EINVAL;
	}
	return -1;
}

#define getpriority fixture_priority_sentinel
#endif

#endif

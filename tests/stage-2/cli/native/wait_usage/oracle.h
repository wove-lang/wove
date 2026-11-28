// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_WAIT_USAGE_ORACLE_H
#define WOVE_TEST_WAIT_USAGE_ORACLE_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/wait.h>

#if defined(__APPLE__)
typedef int FixtureMicros;
#else
typedef long FixtureMicros;
#endif

// A successful reaping call must replace both invalid time values.
static inline void fixture_seed_usage(struct rusage *usage)
{
	*usage = (struct rusage){
		.ru_utime = {.tv_sec = -1, .tv_usec = -1},
		.ru_stime = {.tv_sec = -1, .tv_usec = -1},
	};
}

// Compare the four Wove projections with the actual C fields.
static inline bool fixture_usage(const struct rusage *usage, long user_sec, FixtureMicros user_usec,
	long system_sec, FixtureMicros system_usec)
{
	return user_sec == usage->ru_utime.tv_sec && user_sec >= 0
		&& user_usec == usage->ru_utime.tv_usec && user_usec >= 0 && user_usec < 1000000
		&& system_sec == usage->ru_stime.tv_sec && system_sec >= 0
		&& system_usec == usage->ru_stime.tv_usec && system_usec >= 0 && system_usec < 1000000;
}

// The named child must already be gone after the selected wait call.
static inline bool fixture_reaped(pid_t child)
{
	errno = 0;
	return waitpid(child, NULL, 0) == (pid_t)-1 && errno == ECHILD;
}

// Use the platform's exact interpretation of the child's exit status.
static inline bool fixture_exit_matches(int32_t status, int32_t expected)
{
	return WIFEXITED(status) && WEXITSTATUS(status) == expected;
}

#ifdef WCOREDUMP
// Compare synthetic status words without producing a core dump.
static inline int32_t fixture_core_mask(int32_t status)
{
	_Static_assert(_Generic(WCOREDUMP(status), int: 1, default: 0), "WCOREDUMP returns an int mask");
	return WCOREDUMP(status);
}
#endif

#endif

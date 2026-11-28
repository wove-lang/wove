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
typedef int FixtureLimitKind;
typedef int FixtureMicros;
#else
typedef __rusage_who_t FixtureWho;
typedef __priority_which_t FixturePriorityWhich;
typedef __rlimit_resource_t FixtureLimitKind;
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


_Static_assert(sizeof(rlim_t) == 8 && (rlim_t)-1 > (rlim_t)0, "unsigned 64-bit rlim_t");
_Static_assert(sizeof(struct rlimit) == 16 && _Alignof(struct rlimit) == 8, "rlimit record layout");
_Static_assert(offsetof(struct rlimit, rlim_cur) == 0 && offsetof(struct rlimit, rlim_max) == 8,
	"rlimit member offsets");
_Static_assert(_Generic(((struct rlimit *)0)->rlim_cur, rlim_t: 1, default: 0)
	&& _Generic(((struct rlimit *)0)->rlim_max, rlim_t: 1, default: 0), "rlimit member types");
_Static_assert(_Generic(&getrlimit, int (*)(FixtureLimitKind, struct rlimit *): 1, default: 0),
	"getrlimit prototype");
_Static_assert(_Generic(&setrlimit, int (*)(FixtureLimitKind, const struct rlimit *): 1, default: 0),
	"setrlimit prototype");

/* Verify every standard selector against the selected header. */
static inline bool fixture_limit_selectors(FixtureLimitKind address, FixtureLimitKind core,
	FixtureLimitKind cpu, FixtureLimitKind data, FixtureLimitKind file_size, FixtureLimitKind files,
	FixtureLimitKind stack)
{
	return address == RLIMIT_AS && core == RLIMIT_CORE && cpu == RLIMIT_CPU && data == RLIMIT_DATA
		&& file_size == RLIMIT_FSIZE && files == RLIMIT_NOFILE && stack == RLIMIT_STACK;
}

/* Compare all three exact unsigned sentinels, including the SDK's infinity bit pattern. */
static inline bool fixture_limit_sentinels(rlim_t infinity, rlim_t saved_current, rlim_t saved_maximum)
{
#if defined(__APPLE__)
	const rlim_t expected = (UINT64_C(1) << 63) - 1;
#else
	const rlim_t expected = UINT64_MAX;
#endif
	return infinity == RLIM_INFINITY && infinity == expected
		&& saved_current == RLIM_SAVED_CUR && saved_current == expected
		&& saved_maximum == RLIM_SAVED_MAX && saved_maximum == expected;
}

/* Compare both Wove projections with an independent native read of the process limits. */
static inline bool fixture_limit_matches(FixtureLimitKind resource, rlim_t current, rlim_t maximum)
{
	struct rlimit native;
	return getrlimit(resource, &native) == 0 && native.rlim_cur == current && native.rlim_max == maximum;
}


#if defined(__GLIBC__) && defined(__USE_LARGEFILE64)
_Static_assert(sizeof(rlim64_t) == 8 && (rlim64_t)-1 > (rlim64_t)0, "unsigned 64-bit rlim64_t");
_Static_assert(sizeof(struct rlimit64) == 16 && _Alignof(struct rlimit64) == 8, "rlimit64 layout");
_Static_assert(offsetof(struct rlimit64, rlim_cur) == 0 && offsetof(struct rlimit64, rlim_max) == 8,
	"rlimit64 member offsets");
_Static_assert(_Generic(((struct rlimit64 *)0)->rlim_cur, rlim64_t: 1, default: 0)
	&& _Generic(((struct rlimit64 *)0)->rlim_max, rlim64_t: 1, default: 0), "rlimit64 member types");
_Static_assert(_Generic(&getrlimit64, int (*)(FixtureLimitKind, struct rlimit64 *): 1, default: 0),
	"getrlimit64 prototype");
_Static_assert(_Generic(&setrlimit64, int (*)(FixtureLimitKind, const struct rlimit64 *): 1, default: 0),
	"setrlimit64 prototype");

#if defined(__USE_GNU)
_Static_assert(_Generic(&prlimit64,
	int (*)(__pid_t, enum __rlimit_resource, const struct rlimit64 *, struct rlimit64 *): 1, default: 0),
	"prlimit64 prototype");
#endif

/* Compare the selected explicit sentinel with the exact unsigned native value. */
static inline bool fixture_limit64_sentinel(rlim64_t value)
{
	return value == RLIM64_INFINITY && value == UINT64_MAX;
}

/* Compare Wove's two projections with an independent native call. */
static inline bool fixture_limit64_matches(FixtureLimitKind resource, rlim64_t current, rlim64_t maximum)
{
	struct rlimit64 native;
	return getrlimit64(resource, &native) == 0 && native.rlim_cur == current && native.rlim_max == maximum;
}
#endif


#if defined(RLIMIT_RSS) && defined(RLIMIT_MEMLOCK) && defined(RLIMIT_NPROC) && defined(RLIM_NLIMITS)
_Static_assert(_Generic((RLIM_NLIMITS), int: 1, default: 0), "native signed int limit count");

/* Compare the public selectors common to the two full headers and their native count. */
static inline bool fixture_extended_limit_selectors(FixtureLimitKind rss, FixtureLimitKind memlock,
	FixtureLimitKind nproc, int32_t count)
{
#if defined(__APPLE__)
	const int expected_count = 9;
#else
	const int expected_count = 16;
#endif
	return rss == RLIMIT_RSS && memlock == RLIMIT_MEMLOCK && nproc == RLIMIT_NPROC
		&& count == RLIM_NLIMITS && count == expected_count;
}
#endif

#if defined(__GLIBC__)
_Static_assert(_Generic((RLIMIT_NLIMITS), int: 1, default: 0), "native signed int glibc limit count");

/* Compare all additional glibc selector values and its second count spelling. */
static inline bool fixture_glibc_limit_selectors(FixtureLimitKind ofile, FixtureLimitKind locks,
	FixtureLimitKind sigpending, FixtureLimitKind msgqueue, FixtureLimitKind nice_limit,
	FixtureLimitKind rtprio, FixtureLimitKind rttime, int32_t count)
{
	return ofile == RLIMIT_OFILE && locks == RLIMIT_LOCKS && sigpending == RLIMIT_SIGPENDING
		&& msgqueue == RLIMIT_MSGQUEUE && nice_limit == RLIMIT_NICE && rtprio == RLIMIT_RTPRIO
		&& rttime == RLIMIT_RTTIME && count == RLIMIT_NLIMITS && count == RLIM_NLIMITS;
}
#endif


#if defined(__APPLE__) && defined(IOPOL_TYPE_DISK)
_Static_assert(_Generic(&getiopolicy_np, int (*)(int, int): 1, default: 0), "getiopolicy_np prototype");
_Static_assert(_Generic(&setiopolicy_np, int (*)(int, int, int): 1, default: 0), "setiopolicy_np prototype");

/* Compare the disk, scope, and policy operands available in earlier Darwin SDKs. */
static inline bool fixture_iopol_disk_values(int32_t io_type, int32_t process, int32_t thread,
	int32_t throttle, int32_t passive)
{
	return io_type == IOPOL_TYPE_DISK && process == IOPOL_SCOPE_PROCESS && thread == IOPOL_SCOPE_THREAD
		&& throttle == IOPOL_THROTTLE && passive == IOPOL_PASSIVE;
}

/* Check the observed policy with a separate C call at the same type and scope. */
static inline bool fixture_iopol_matches(int32_t io_type, int32_t scope, int32_t observed)
{
	errno = 0;
	const int native = getiopolicy_np(io_type, scope);
	return errno == 0 && native == observed;
}
#endif

#ifdef WOVE_TEST_IOPOL_ALL
/* Compare each Wove constant with its live SDK expression in declaration order. */
static inline bool fixture_iopol_value(int32_t index, int32_t value)
{
	const int expected[] = {
		IOPOL_TYPE_DISK,
		IOPOL_TYPE_VFS_ATIME_UPDATES,
		IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES,
		IOPOL_TYPE_VFS_STATFS_NO_DATA_VOLUME,
		IOPOL_TYPE_VFS_TRIGGER_RESOLVE,
		IOPOL_TYPE_VFS_CONTENT_PROTECTION,
		IOPOL_TYPE_VFS_IGNORE_PERMISSIONS,
		IOPOL_TYPE_VFS_SKIP_MTIME_UPDATE,
		IOPOL_TYPE_VFS_ALLOW_LOW_SPACE_WRITES,
		IOPOL_TYPE_VFS_DISALLOW_RW_FOR_O_EVTONLY,
		IOPOL_TYPE_VFS_ENTITLED_RESERVE_ACCESS,
		IOPOL_TYPE_VFS_IGNORE_CONTENT_PROTECTION,
		IOPOL_SCOPE_PROCESS,
		IOPOL_SCOPE_THREAD,
		IOPOL_SCOPE_DARWIN_BG,
		IOPOL_DEFAULT,
		IOPOL_IMPORTANT,
		IOPOL_PASSIVE,
		IOPOL_THROTTLE,
		IOPOL_UTILITY,
		IOPOL_STANDARD,
		IOPOL_APPLICATION,
		IOPOL_NORMAL,
		IOPOL_ATIME_UPDATES_DEFAULT,
		IOPOL_ATIME_UPDATES_OFF,
		IOPOL_MATERIALIZE_DATALESS_FILES_DEFAULT,
		IOPOL_MATERIALIZE_DATALESS_FILES_OFF,
		IOPOL_MATERIALIZE_DATALESS_FILES_ON,
		IOPOL_MATERIALIZE_DATALESS_FILES_ORIG,
		IOPOL_MATERIALIZE_DATALESS_FILES_BASIC_MASK,
		IOPOL_VFS_STATFS_NO_DATA_VOLUME_DEFAULT,
		IOPOL_VFS_STATFS_FORCE_NO_DATA_VOLUME,
		IOPOL_VFS_TRIGGER_RESOLVE_DEFAULT,
		IOPOL_VFS_TRIGGER_RESOLVE_OFF,
		IOPOL_VFS_CONTENT_PROTECTION_DEFAULT,
		IOPOL_VFS_CONTENT_PROTECTION_IGNORE,
		IOPOL_VFS_CONTENT_PROTECTION_STRICT,
		IOPOL_VFS_IGNORE_PERMISSIONS_OFF,
		IOPOL_VFS_IGNORE_PERMISSIONS_ON,
		IOPOL_VFS_SKIP_MTIME_UPDATE_OFF,
		IOPOL_VFS_SKIP_MTIME_UPDATE_ON,
		IOPOL_VFS_SKIP_MTIME_UPDATE_IGNORE,
		IOPOL_VFS_ALLOW_LOW_SPACE_WRITES_OFF,
		IOPOL_VFS_ALLOW_LOW_SPACE_WRITES_ON,
		IOPOL_VFS_DISALLOW_RW_FOR_O_EVTONLY_DEFAULT,
		IOPOL_VFS_DISALLOW_RW_FOR_O_EVTONLY_ON,
		IOPOL_VFS_NOCACHE_WRITE_FS_BLKSIZE_DEFAULT,
		IOPOL_VFS_NOCACHE_WRITE_FS_BLKSIZE_ON,
		IOPOL_VFS_ENTITLED_RESERVE_ACCESS_OFF,
		IOPOL_VFS_ENTITLED_RESERVE_ACCESS_ON,
	};
	return index >= 0 && (size_t)index < sizeof(expected) / sizeof(expected[0])
		&& expected[index] == value;
}
#endif

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

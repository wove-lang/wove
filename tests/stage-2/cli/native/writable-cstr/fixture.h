// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_NATIVE_STRING_LVALUE_FIXTURE_H
#define WOVE_NATIVE_STRING_LVALUE_FIXTURE_H

#include <stdint.h>
#include <string.h>

#if defined(WOVE_FIXTURE_GNU_NAMES) && defined(__linux__)
#include <errno.h>
#endif

#if defined(WOVE_FIXTURE_GNU_NAMES) && defined(__GLIBC__)
#define fixture_full program_invocation_name
#define fixture_short program_invocation_short_name
#elif defined WOVE_FIXTURE_CONST_NAME
static const char *fixture_full = "read only";
#else
static char initial_full[] = "original/full";
static char initial_short[] = "original";
static char *fixture_full = initial_full;
static char *fixture_short = initial_short;
#endif

#ifndef WOVE_FIXTURE_CONST_NAME
static char *saved_full;
static char *saved_short;
static char *first_full;
static char *first_short;

static inline void save_names(void) {
	saved_full = fixture_full;
	saved_short = fixture_short;
}

static inline int32_t mutate_names(void) {
	if (fixture_full == saved_full || fixture_short == saved_short ||
		strcmp(fixture_full, "front/tool") || strcmp(fixture_short, "tool"))
		return 1;
	first_full = fixture_full;
	first_short = fixture_short;
	fixture_full[0] = 'F';
	fixture_short[0] = 'T';
	return 0;
}

static inline int32_t repeated_names(void) {
	return fixture_full == first_full && fixture_short == first_short &&
		strcmp(fixture_full, "Front/tool") == 0 && strcmp(fixture_short, "Tool") == 0;
}

static inline int32_t alternate_names(void) {
	return fixture_full != first_full && fixture_short != first_short &&
		strcmp(fixture_full, "other/path") == 0 && strcmp(fixture_short, "path") == 0 &&
		strcmp(first_full, "Front/tool") == 0 && strcmp(first_short, "Tool") == 0;
}

static inline void clear_short(void) {
	fixture_short = NULL;
}

static inline int32_t restore_names(void) {
	fixture_full = saved_full;
	fixture_short = saved_short;
	return fixture_full == saved_full && fixture_short == saved_short;
}
#endif

#endif

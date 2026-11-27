// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

static unsigned char error_path[] = {'a', '/', 255, 0};

// Verify that the receiver can read the original bytes up to the terminator.
static inline bool fixture_original_error_path(const void *path, int32_t length) {
	return path == error_path && length == 3;
}

#ifdef FIXTURE_WRONG_ERROR_QUALIFIER
typedef int (*FixtureErrorReceiver)(char *, int);
#else
typedef int (*FixtureErrorReceiver)(const char *, int);
#endif

// Pass an actual native null or invoke the supplied receiver twice.
static inline int fixture_error(FixtureErrorReceiver report) {
	// Expose whether Wove supplied the native null.
	if (report == NULL) {
		return 17;
	}
	char *path = (char *)error_path;
	// Require the first signed error and the receiver's unchanged result.
	if (report(path, -13) != -21) {
		return 90;
	}
	return report(path, 13);
}

// Declare a retained string contract for the lifetime rejection case.
int fixture_retain_error_path(const char *path);

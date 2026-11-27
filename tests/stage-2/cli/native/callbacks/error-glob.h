// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <errno.h>
#include <glob.h>
#include <unistd.h>

// Expose the platform's native signed error for the Wove receiver.
static inline int fixture_loop_error(void) {
	return ELOOP;
}

// Invoke the real glob error callback on a self-referential symlink.
static inline int fixture_glob_error(int (*report)(const char *, int)) {
	// The loop fails independently of the test runner's directory permissions.
	if (symlink("loop", "loop") != 0) {
		return 95;
	}
	glob_t result = {0};
	int status = glob("loop/*", 0, report, &result);
	globfree(&result);
	// Remove the entry even when the SDK reports an unexpected result.
	if (unlink("loop") != 0) {
		return 96;
	}
	return status == GLOB_ABORTED ? 42 : 97;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_PATH_IDENTITY_H
#define WOVE_TEST_PATH_IDENTITY_H

#include <stdint.h>
#include <sys/stat.h>

// Check that the copied absolute path identifies the original relative file.
static inline int32_t same_file(const char *canonical, const char *original) {
	struct stat expected, actual;
	if (canonical[0] != '/' || stat(original, &expected) != 0 || stat(canonical, &actual) != 0) { return 1; }
	return expected.st_dev == actual.st_dev && expected.st_ino == actual.st_ino ? 42 : 2;
}

#endif

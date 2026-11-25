// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_BORROWED_STRINGS_H
#define WOVE_TEST_BORROWED_STRINGS_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

// Mutate both call-scoped strings and return a borrowed alias inside the first.
static inline const char *edit_strings(const char *tag, char *first, int32_t missing, char *second) {
	// The copies must contain the original bytes and an implicit terminator.
	if (tag[0] != 'p' || tag[1] != 0 || first[0] != 'a' || first[4] != 0 ||
		second[0] != 'x' || second[2] != 0) {
		errno = EINVAL;
		return NULL;
	}
	first[0] = 'M';
	first[1] = 'B';
	first[2] = 0;
	first[3] = 'T';
	second[0] = 'N';
	second[1] = 'Y';
	errno = missing ? ENOENT : ERANGE;
	return missing ? NULL : first + 1;
}

// A present empty string is borrowed from immutable static storage.
static inline const char *empty_text(void) {
	return "";
}

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_CSTR_OFFSET_FIXTURE_H
#define WOVE_CSTR_OFFSET_FIXTURE_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Write two positions into a checked string and distinguish a null preceding argument.
static inline int32_t fixture_positions(const char *before, char **first, const char *text, char **last) {
	*first = (char *)text + 1;
	*last = (char *)text + strlen(text);
	errno = EDOM;
	return before == NULL ? -7 : 7;
}

// Return pointers before and after the selected scratch slice, foreign storage, or no write.
static inline int32_t fixture_invalid(const char *before, const char *text, const char *after,
		char **stop, int32_t scenario) {
	static char foreign[] = "foreign";
	(void)text;
	switch (scenario) {
	case 0: *stop = (char *)after; break;
	case 1: *stop = (char *)before; break;
	case 2: *stop = foreign; break;
	default: break;
	}
	return 42;
}

// An incompatible end-pointer qualification must reach Clang's ordinary prototype diagnostic.
static inline int32_t fixture_wrong(const char *text, const char **stop) {
	*stop = text;
	return 42;
}
#endif

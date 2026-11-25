// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_SNAPSHOT_FIELD_H
#define WOVE_TEST_SNAPSHOT_FIELD_H

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include "wove_runtime.h"

struct snapshot_record {
	char name[4];
	char after;
};

struct pointer_record {
	const char *name;
};

struct flexible_record {
	int unused;
	char name[];
};

struct const_record {
	char name[4];
};

// Return reusable storage, an empty name, a null result, or an unterminated field.
static inline struct snapshot_record *snapshot_read(int32_t mode) {
	static struct snapshot_record record;
	errno = EDOM;
	// A missing result must leave the callee's error indicator intact.
	if (mode < 0) { return NULL; }
	// Keep a terminator immediately after the field even when its four bytes are full.
	if (mode == 3) {
		record = (struct snapshot_record){{'a', 'b', 'c', 'd'}, '\0'};
	} else {
		record = (struct snapshot_record){{mode == 0 ? '\0' : (char)('a' + mode), 'x', 'y', '\0'}, '\0'};
	}
	return &record;
}

// Keep the native result typed as a pointer to a const record.
static inline const struct const_record *snapshot_const(void) {
	static const struct const_record record = {{'c', 'o', 'n', '\0'}};
	return &record;
}

// Return pointer-valued storage for a rejected field-shape check.
static inline struct pointer_record *snapshot_pointer(void) {
	static struct pointer_record record = {"bad"};
	return &record;
}

// Expose a flexible member shape without any actual instance.
static inline struct flexible_record *snapshot_flexible(void) {
	return NULL;
}

struct snapshot_nominal_dirent {
	char d_name[4];
};

struct snapshot_extended_dirent {
	struct snapshot_nominal_dirent entry;
	char extension[6];
};

// Model a directory result whose terminated name continues into the returned record.
static inline const struct snapshot_nominal_dirent *snapshot_readdir(int32_t *reads) {
	static const struct snapshot_extended_dirent record = {{{'l', 'o', 'n', 'g'}}, {'-', 'n', 'a', 'm', 'e', '\0'}};
	_Static_assert(offsetof(struct snapshot_extended_dirent, extension) == sizeof(record.entry.d_name),
		"the test record extends the nominal name without padding");
	++*reads;
	errno = EDOM;
	return &record.entry;
}

// Exercise the same directory adapter as the bundled provider with an extended record.
static inline const char *snapshot_directory_name(void) {
	int32_t reads = 0;
#define readdir(directory) snapshot_readdir(directory)
	const char *name = WVRT_READDIR_NAME(&reads);
#undef readdir
	return reads == 1 ? name : NULL;
}

#endif

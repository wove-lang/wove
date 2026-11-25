// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_SNAPSHOT_FIELD_H
#define WOVE_TEST_SNAPSHOT_FIELD_H

#include <errno.h>
#include <stdint.h>
#include <stddef.h>

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

#endif

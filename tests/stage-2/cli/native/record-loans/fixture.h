// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct loan_group {
	uint32_t gid;
	char *name;
	char *password;
	char **members;
};

// Supply an ordinary record and byte buffer, with one targeted contract violation per mode.
static inline int32_t fixture_group(int32_t mode, struct loan_group *record, char *bytes,
	size_t length, struct loan_group **result) {
	static struct loan_group other;
	static char foreign[] = "foreign";
	// The fixture writes only the range designated by the native length argument.
	if (length < 72) {
		*result = NULL;
		return 34;
	}
	memset(bytes, 'x', length);
	memcpy(bytes + 32, "name", 5);
	memcpy(bytes + 48, "password", 9);
	memcpy(bytes + 64, "row", 4);
	char *member = bytes + 64;
	char *end = NULL;
	memcpy(bytes + 1, &member, sizeof(member));
	memcpy(bytes + 1 + sizeof(member), &end, sizeof(end));
	*record = (struct loan_group){42, bytes + 32, bytes + 48, (char **)(void *)(bytes + 1)};
	*result = record;
	// Reject a different record even when its result pointer has the right native type.
	if (mode == 1) { *result = &other; }
	// A string from a different allocation cannot be part of this result.
	if (mode == 2) { record->name = foreign; }
	// Passing a shorter native length protects the rest of the same allocation.
	if (mode == 3) { record->name = bytes + length; }
	// An in-range pointer slot may still lead into unrelated storage.
	if (mode == 4) {
		char *address = foreign;
		memcpy(bytes + 1, &address, sizeof(address));
	}
	// The last byte cannot contain a complete pointer slot.
	if (mode == 5) { record->members = (char **)(void *)(bytes + length - 1); }
	// A C string that starts in range still needs a terminator in range.
	if (mode == 6) { record->name = bytes + length - 1; }
	// A missing null slot at the end of the backing range must terminate the check.
	if (mode == 7) {
		char *tail = bytes + length - sizeof(member);
		memcpy(tail, &member, sizeof(member));
		record->members = (char **)(void *)tail;
	}
	return 0;
}

// Read only a complete native record. The adapter validates it before C can load pointers.
static inline int32_t consume_group(const struct loan_group *record) {
	return record->gid == 42 ? 0 : 1;
}

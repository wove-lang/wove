// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum { FIXTURE_NO_STATUS = 7, FIXTURE_SKIP_SUBTREE = 11 };
struct fixture_status { int value; };
struct fixture_walk { int base; int level; };

static const unsigned char original_path[] = {'a', '/', 255, 0};

// Confirm the callback borrowed the original exact-length byte sequence.
static inline bool fixture_original(const void *path, int32_t length) {
	return path == original_path && length == 3;
}

// Invoke a three-argument receiver and pass through its return code.
static inline int fixture_direct(int (*visit)(const char *, const struct fixture_status *, int)) {
	struct fixture_status status = {42};
	return visit((const char *)original_path, &status, 0);
}

// Allow the negative case to verify the fourth parameter's exact C qualifier.
#ifdef FIXTURE_WRONG_WALK_QUALIFIER
#define FIXTURE_WALK_QUALIFIER const
#else
#define FIXTURE_WALK_QUALIFIER
#endif

// Reuse both native records, including a non-null but unavailable status.
static inline int fixture_extended(const char *root,
	int (*visit)(const char *, const struct fixture_status *, int, FIXTURE_WALK_QUALIFIER struct fixture_walk *)) {
	// Require the normal outgoing C string conversion as well.
	if (strcmp(root, "root") != 0) {
		return 101;
	}
	struct fixture_status status = {20};
	struct fixture_walk walk = {2, 0};
	const char *path = (const char *)original_path;
	// Treat the receiver's explicit traversal action as a native control value.
	if (visit(path, &status, 0, &walk) != FIXTURE_SKIP_SUBTREE) {
		return 102;
	}
	walk.level = 1;
	// FTW_NS-like cases can pass a non-null pointer with no usable record.
	if (visit(path, &status, FIXTURE_NO_STATUS, &walk) != 0) {
		return 103;
	}
	// A native null is absent even under an ordinary visit code.
	if (visit(path, NULL, 2, &walk) != 0) {
		return 104;
	}
	status.value = 42;
	walk.level = 2;
	return visit(path, &status, 1, &walk);
}

// Declare the exact retained-string contract for the negative Wove case.
int fixture_retain(const char *path);

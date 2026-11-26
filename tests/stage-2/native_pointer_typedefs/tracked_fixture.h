// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_POINTER_TYPEDEF_TRACKED_H
#define WOVE_TEST_POINTER_TYPEDEF_TRACKED_H

#include <stdint.h>

struct TrackedHandle { int32_t live; };
typedef struct TrackedHandle *TrackedOwner;

static struct TrackedHandle handles[4];
static int32_t created;
static int32_t dropped;

// Give each acquisition its own slot so duplicate drops can be detected.
static inline TrackedOwner acquire(void) {
	TrackedOwner owner = &handles[created++];
	owner->live = 1;
	return owner;
}

// Count unique destructions and make a repeated destruction fail the fixture.
static inline int32_t release(TrackedOwner owner) {
	dropped += owner->live ? 1 : 100;
	owner->live = 0;
	return 0;
}

// A checked owner must call the declared function even when this macro exists.
#define release(value) ((void)(value))

// Expose the number of destructions to the Wove fixture.
static inline int32_t drop_count(void) {
	return dropped;
}

#endif

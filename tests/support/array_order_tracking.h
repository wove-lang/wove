// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_ARRAY_ORDER_TRACKING_H
#define WOVE_TEST_ARRAY_ORDER_TRACKING_H

// The native fixture supplies the expected descriptor drop order.
#include <stdio.h>
#include <stdlib.h>

#undef main
int wove_test_main(int argc, char **argv);

typedef struct {
	void *descriptor;
	void *backing;
} Allocation;

static Allocation live[sizeof wove_track_drop_order / sizeof wove_track_drop_order[0]];
static size_t descriptors;
static size_t backings;
static size_t dropped;

// Report the allocation invariant that failed.
static _Noreturn void fail(const char *reason) {
	fprintf(stderr, "%s\n", reason);
	exit(EXIT_FAILURE);
}

// Register each array descriptor before its own backing allocation.
void *tracked_malloc(size_t size) {
	// The current descriptor must get its backing before another starts.
	if (descriptors == sizeof live / sizeof live[0] || descriptors != backings) {
		fail("unexpected array descriptor");
	}
	Allocation *allocation = &live[descriptors++];
	allocation->descriptor = malloc(size);
	return allocation->descriptor;
}

// Associate one backing block with the most recent descriptor.
void *tracked_calloc(size_t count, size_t size) {
	if (backings + 1 != descriptors) { fail("unexpected array backing"); }
	Allocation *allocation = &live[backings++];
	allocation->backing = calloc(count, size);
	return allocation->backing;
}

// Require each owned backing and then its descriptor to be released once.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	if (dropped == sizeof live / sizeof live[0]) { fail("extra array drop"); }
	Allocation *allocation = &live[wove_track_drop_order[dropped]];
	// Retain only live addresses, even when the allocator reuses a retired block.
	if (pointer == allocation->backing) {
		allocation->backing = NULL;
	} else if (pointer == allocation->descriptor && allocation->backing == NULL) {
		allocation->descriptor = NULL;
		++dropped;
	} else {
		fail("unexpected recursive cleanup order");
	}
	free(pointer);
}

// Verify every allocation was released across both exercises.
int main(int argc, char **argv) {
	int result = wove_test_main(argc, argv);
	// The Wove result and the allocation ledger must both agree.
	if (descriptors != sizeof live / sizeof live[0] || backings != descriptors || dropped != descriptors) {
		fail("missing recursive array cleanup");
	}
	return result;
}

#endif

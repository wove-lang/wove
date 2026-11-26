// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#undef main

typedef struct {
	uint32_t key;
	int32_t hits;
} RowLayout;

static void *live_items;
static void *retiring_items;
static void *descriptor;
static int backing_allocations;
static int backing_releases;
static int failed_allocations;
static int descriptor_releases;

// Only the emitted array's descriptor uses the tracked runtime in this fixture.
void *tracked_malloc(size_t size) {
	assert(size == sizeof(wvrt_array) && descriptor == NULL);
	descriptor = malloc(size);
	return descriptor;
}

// Fail the oversized reservation once and record each successfully grown allocation.
void *tracked_calloc(size_t count, size_t size) {
	assert(size == sizeof(RowLayout));
	// Only this reservation simulates a host allocation failure.
	if (count == 64) {
		++failed_allocations;
		errno = ENOMEM;
		return NULL;
	}
	assert(retiring_items == NULL);
	retiring_items = live_items;
	live_items = calloc(count, size);
	assert(live_items != NULL);
	++backing_allocations;
	return live_items;
}

// Check that replaced storage is retired before the current backing and descriptor.
void tracked_free(void *pointer) {
	// A successful resize frees exactly the previous backing.
	if (pointer == retiring_items) {
		retiring_items = NULL;
		++backing_releases;
	// Final cleanup frees the latest backing separately from the descriptor.
	} else if (pointer == live_items) {
		assert(retiring_items == NULL);
		live_items = NULL;
		++backing_releases;
	// The final release must refer to the original array descriptor.
	} else {
		assert(pointer == descriptor && live_items == NULL);
		descriptor = NULL;
		++descriptor_releases;
	}
	free(pointer);
}

int wove_test_main(int argc, char **argv);

// Verify geometric growth, one failed reserve, and exactly-once cleanup.
int main(int argc, char **argv) {
	int status = wove_test_main(argc, argv);
	assert(backing_allocations == 4 && backing_releases == backing_allocations);
	assert(failed_allocations == 1 && descriptor_releases == 1);
	assert(live_items == NULL && retiring_items == NULL && descriptor == NULL);
	return status;
}

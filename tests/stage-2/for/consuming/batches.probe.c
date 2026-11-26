// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <assert.h>
#include <stdlib.h>

#undef main

int wove_test_main(int argc, char **argv);

typedef struct {
	void *descriptor;
	void *items;
} Allocation;

// Exhaustion, continue then break with one saved row, and both kinds of return.
static const size_t drop_order[] = {
	1, 2, 3, 0,
	6, 8, 5, 7, 4,
	10, 11, 12, 9,
	15, 16, 13, 14
};
enum { allocation_count = sizeof(drop_order) / sizeof(drop_order[0]) };

static Allocation allocations[allocation_count];
static size_t descriptors;
static size_t initialized;
static size_t released;

// Each observed allocation must correspond to one original vector or byte row.
void *tracked_malloc(size_t size) {
	assert(size == sizeof(wvrt_array) && descriptors == initialized && descriptors < allocation_count);
	Allocation *entry = &allocations[descriptors++];
	entry->descriptor = malloc(size);
	assert(entry->descriptor != NULL);
	return entry->descriptor;
}

// Check the exact storage shape and exclude copies of the original row allocations.
void *tracked_calloc(size_t count, size_t size) {
	assert(initialized < descriptors);
	const bool batch = initialized == 0 || initialized == 5 || initialized == 9 || initialized == 13;
	assert(count == (batch ? 3U : 1U));
	assert(size == ((batch || initialized == 4) ? sizeof(wvrt_array *) : sizeof(uint8_t)));
	Allocation *entry = &allocations[initialized++];
	entry->items = calloc(count, size);
	assert(entry->items != NULL);
	return entry->items;
}

// A released owner must be the expected current item, suffix item, or vector.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	assert(released < allocation_count);
	Allocation *entry = &allocations[drop_order[released]];
	// Finish the backing release before retiring the descriptor.
	if (pointer == entry->items) {
		entry->items = NULL;
	} else {
		assert(pointer == entry->descriptor && entry->items == NULL);
		entry->descriptor = NULL;
		++released;
	}
	free(pointer);
}

// Every exit in the fixture must retire all the allocations it owns exactly once.
int main(int argc, char **argv) {
	int status = wove_test_main(argc, argv);
	assert(descriptors == allocation_count && initialized == allocation_count && released == allocation_count);
	return status;
}

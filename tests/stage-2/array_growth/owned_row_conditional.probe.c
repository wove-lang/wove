// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <assert.h>
#include <stdlib.h>

#undef main

int wove_test_main(int argc, char **argv);

enum { taken_vector, taken_row, kept_vector, kept_row, fallback, allocation_count };

typedef struct {
	void *descriptor;
	void *items;
} Allocation;

static Allocation allocations[allocation_count];
static size_t descriptors;
static size_t initialized;
static size_t released;

// Removing a row lets the vector drop first. Otherwise the vector still drops its row.
static const size_t drop_order[] = {taken_vector, taken_row, kept_row, kept_vector, fallback};

// Register the original owner before allocating its backing storage.
void *tracked_malloc(size_t size) {
	assert(size == sizeof(wvrt_array) && descriptors == initialized && descriptors < allocation_count);
	Allocation *entry = &allocations[descriptors++];
	entry->descriptor = malloc(size);
	assert(entry->descriptor != NULL);
	return entry->descriptor;
}

// Only the false branch allocates a third owner for the fallback.
void *tracked_calloc(size_t count, size_t size) {
	assert(initialized < descriptors);
	const bool vector = initialized == taken_vector || initialized == kept_vector;
	assert(count == (initialized == fallback ? 42U : 1U));
	assert(size == (vector ? sizeof(wvrt_array *) : sizeof(uint8_t)));
	Allocation *entry = &allocations[initialized++];
	entry->items = calloc(count, size);
	assert(entry->items != NULL);
	return entry->items;
}

// Match each release to the owner due for destruction, with backing storage first.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	assert(released < allocation_count);
	Allocation *entry = &allocations[drop_order[released]];
	// Release one backing block and then its original descriptor.
	if (pointer == entry->items) {
		entry->items = NULL;
	} else {
		assert(pointer == entry->descriptor && entry->items == NULL);
		entry->descriptor = NULL;
		++released;
	}
	free(pointer);
}

// Check that both calls returned their values and released exactly five owners between them.
int main(int argc, char **argv) {
	int status = wove_test_main(argc, argv);
	assert(descriptors == allocation_count && initialized == allocation_count && released == allocation_count);
	return status;
}

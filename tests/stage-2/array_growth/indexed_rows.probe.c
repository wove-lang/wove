// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <assert.h>
#include <stdlib.h>

#undef main
int wove_test_main(int argc, char **argv);

enum { batches, left, right, alpha, beta, spare, allocation_count };

typedef struct {
	void *descriptor;
	void *backing;
} Allocation;

static Allocation live[allocation_count];
static size_t descriptors;
static size_t initialized;
static size_t resizes;
static size_t dropped;
static void *retiring_backing;
static const size_t drop_order[] = {spare, alpha, left, beta, right, batches};

// Track each descriptor before the runtime allocates its backing once.
void *tracked_malloc(size_t size) {
	assert(size == sizeof(wvrt_array) && descriptors == initialized && descriptors < allocation_count);
	Allocation *entry = &live[descriptors++];
	entry->descriptor = malloc(size);
	assert(entry->descriptor != NULL);
	return entry->descriptor;
}

// Attribute each resize to the selected row descriptor.
void *tracked_calloc(size_t count, size_t size) {
	// Creation pairs the new backing with the most recent descriptor.
	if (initialized < descriptors) {
		assert(count == (initialized == batches ? 2U : 1U));
		Allocation *entry = &live[initialized++];
		entry->backing = calloc(count, size);
		assert(entry->backing != NULL);
		return entry->backing;
	}
	assert(resizes < 2 && retiring_backing == NULL);
	const size_t row = resizes++ == 0 ? left : beta;
	assert(count == (row == left ? 16U : 32U));
	assert(size == (row == left ? sizeof(wvrt_array *) : sizeof(int32_t)));
	retiring_backing = live[row].backing;
	live[row].backing = calloc(count, size);
	assert(live[row].backing != NULL);
	return live[row].backing;
}

// Growth drops the old backing. Lexical cleanup drops every descriptor once.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	// A resize must leave the row descriptor alive in its original slot.
	if (pointer == retiring_backing) {
		retiring_backing = NULL;
	} else {
		assert(dropped < allocation_count && retiring_backing == NULL);
		Allocation *entry = &live[drop_order[dropped]];
		// Retire each live backing before its descriptor.
		if (pointer == entry->backing) {
			entry->backing = NULL;
		} else {
			assert(pointer == entry->descriptor && entry->backing == NULL);
			entry->descriptor = NULL;
			++dropped;
		}
	}
	free(pointer);
}

// Check the Wove result together with the allocator ledger.
int main(int argc, char **argv) {
	int result = wove_test_main(argc, argv);
	assert(descriptors == allocation_count && initialized == allocation_count);
	assert(dropped == allocation_count && resizes == 2 && retiring_backing == NULL);
	return result;
}

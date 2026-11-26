// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#undef main

int wove_test_main(int argc, char **argv);

enum { rows, first, removed, third, discarded, allocation_count };

typedef struct {
	void *descriptor;
	void *items;
} Allocation;

static Allocation allocations[allocation_count];
static size_t descriptors;
static size_t initialized;
static size_t dropped;
static size_t resizes;
static size_t retired;
static size_t failures;
static void *previous_items;

// Truncation drops the tail immediately. Scope exit owns the removed row separately.
static const size_t drop_order[] = {discarded, removed, first, third, rows};

// Every byte row is created once, before moving its original descriptor into the vector.
void *tracked_malloc(size_t size) {
	assert(size == sizeof(wvrt_array) && descriptors == initialized && descriptors < allocation_count);
	Allocation *entry = &allocations[descriptors++];
	entry->descriptor = malloc(size);
	assert(entry->descriptor != NULL);
	return entry->descriptor;
}

// Track the vector's replacements and inject a failed reservation after the four appends.
void *tracked_calloc(size_t count, size_t size) {
	// After a descriptor allocation, register its initial-sized backing store.
	if (initialized < descriptors) {
		assert(count == 1 && size == (initialized == rows ? sizeof(wvrt_array *) : sizeof(uint8_t)));
		Allocation *entry = &allocations[initialized++];
		entry->items = calloc(count, size);
		assert(entry->items != NULL);
		return entry->items;
	}
	assert(size == sizeof(wvrt_array *) && previous_items == NULL);
	// Fail only the explicit reserve so the same live rows can be checked afterward.
	if (count == 64) {
		assert(initialized == allocation_count && retired == 2);
		++failures;
		errno = ENOMEM;
		return NULL;
	}
	assert(resizes < 2 && count == (resizes == 0 ? 2U : 4U));
	++resizes;
	previous_items = allocations[rows].items;
	allocations[rows].items = calloc(count, size);
	assert(allocations[rows].items != NULL);
	return allocations[rows].items;
}

// Retiring a backing block during growth must leave every row alive.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	// Each growth releases the prior outer backing, with no row descriptor releases.
	if (pointer == previous_items) {
		previous_items = NULL;
		++retired;
	} else {
		assert(dropped < allocation_count && previous_items == NULL);
		Allocation *entry = &allocations[drop_order[dropped]];
		// Destruction must release each current backing before its descriptor.
		if (pointer == entry->items) {
			entry->items = NULL;
		} else {
			assert(pointer == entry->descriptor && entry->items == NULL);
			entry->descriptor = NULL;
			++dropped;
		}
	}
	free(pointer);
}

// The moved local, remaining rows, and truncated suffix must each release exactly once.
int main(int argc, char **argv) {
	int status = wove_test_main(argc, argv);
	assert(initialized == allocation_count && dropped == allocation_count);
	assert(resizes == 2 && retired == resizes && failures == 1 && previous_items == NULL);
	return status;
}

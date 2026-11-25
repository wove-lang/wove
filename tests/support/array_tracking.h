// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_ARRAY_TRACKING_H
#define WOVE_TEST_ARRAY_TRACKING_H

// Include in the fixture that supplies the WOVE_TRACK_* expectations.
#include <stdio.h>
#include <stdlib.h>

static void *descriptor;
static void *backing;
static size_t descriptor_allocations;
static size_t backing_allocations;

int wove_test_main(int argc, char **argv);

// Fail the probe on unexpected allocator activity.
static _Noreturn void fail(const char *message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

// Each tracked owner has one descriptor and completes cleanup before the next allocation.
void *tracked_malloc(size_t size) {
	// Each native fixture specifies how many independent allocations to expect.
	if (descriptor != NULL || backing != NULL || descriptor_allocations == WOVE_TRACK_ARRAYS) {
		fail("unexpected descriptor allocation");
	}
	++descriptor_allocations;
	descriptor = malloc(size);
	return descriptor;
}

// Its zero-initialized backing storage is one separate allocation.
void *tracked_calloc(size_t count, size_t size) {
	// The current descriptor owns one backing block with the fixture's native element layout.
	if (descriptor == NULL || backing != NULL || backing_allocations + 1 != descriptor_allocations ||
			count != WOVE_TRACK_ELEMENTS || size != WOVE_TRACK_ELEMENT_SIZE) {
		fail("unexpected backing storage allocation");
	}
	++backing_allocations;
	backing = calloc(count, size);
	return backing;
}

// Match each release to a live block, and require backing-before-descriptor order.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	// Match only live allocations and release the backing block before its descriptor.
	if (pointer == backing) {
		backing = NULL;
	} else if (pointer == descriptor && backing == NULL) {
		descriptor = NULL;
	} else {
		fail("unexpected or repeated release");
	}
	free(pointer);
}

// Check every expected descriptor and backing block was released before returning the Wove result.
int main(int argc, char **argv) {
	int status = wove_test_main(argc, argv);
	// An exit status alone cannot establish that both original blocks were released.
	if (descriptor_allocations != WOVE_TRACK_ARRAYS || backing_allocations != WOVE_TRACK_ARRAYS ||
			descriptor != NULL || backing != NULL) {
		fail("expected each array allocation to be released");
	}
	return status;
}

#endif

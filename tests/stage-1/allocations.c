// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static void *descriptor;
static void *backing;
static bool allocated_descriptor;
static bool allocated_backing;

int wove_test_main(void);

// Fail the probe on unexpected allocator activity.
static _Noreturn void fail(const char *message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

// An owned byte array has one descriptor allocated with malloc.
void *tracked_malloc(size_t size) {
	if (allocated_descriptor) { fail("second descriptor allocation"); }
	allocated_descriptor = true;
	descriptor = malloc(size);
	return descriptor;
}

// Its zero-initialized backing storage is one separate allocation.
void *tracked_calloc(size_t count, size_t size) {
	// Expect the initial three requested bytes and a single backing allocation.
	if (allocated_backing || count != 3 || size != 1) {
		fail("unexpected backing storage allocation");
	}
	allocated_backing = true;
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

// Check both original allocations were released before returning the Wove result.
int main(void) {
	int status = wove_test_main();
	// An exit status alone cannot establish that both original blocks were released.
	if (!allocated_descriptor || !allocated_backing || descriptor != NULL || backing != NULL) {
		fail("expected one array allocation and its complete release");
	}
	return status;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "faults.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#undef main

static void *live[32];
static int allocations;
static int releases;
static int descriptor_failures;
static int backing_failures;
static bool reject_descriptor;
static int32_t backing_countdown = -1;

// Reject one descriptor allocation before any backing is requested.
void fail_descriptor(void) {
	assert(!reject_descriptor);
	reject_descriptor = true;
}

// Reject a backing allocation after the requested number of successful calls.
void fail_backing_after(int32_t successes) {
	assert(backing_countdown < 0 && successes >= 0);
	backing_countdown = successes;
}

// Register an allocation until its one matching drop.
static void *remember(void *value) {
	assert(value != NULL);
	for (size_t index = 0; index < sizeof live / sizeof live[0]; ++index) {
		if (live[index] == NULL) {
			live[index] = value;
			++allocations;
			return value;
		}
	}
	abort();
}

// Fail only the scheduled descriptor and track successful allocations.
void *tracked_malloc(size_t size) {
	if (reject_descriptor) {
		reject_descriptor = false;
		++descriptor_failures;
		errno = ENOMEM;
		return NULL;
	}
	return remember(malloc(size));
}

// Fail an individual growth step while preserving the existing tracked pointers.
void *tracked_calloc(size_t count, size_t size) {
	if (backing_countdown == 0) {
		backing_countdown = -1;
		++backing_failures;
		errno = ENOMEM;
		return NULL;
	}
	if (backing_countdown > 0) { --backing_countdown; }
	return remember(calloc(count, size));
}

// Accept each release once and ignore a cleared or absent owner.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	for (size_t index = 0; index < sizeof live / sizeof live[0]; ++index) {
		if (live[index] == pointer) {
			live[index] = NULL;
			++releases;
			free(pointer);
			return;
		}
	}
	abort();
}

int wove_test_main(int argc, char **argv);

// Check both exercised failure paths and the complete final ownership ledger.
int main(int argc, char **argv) {
	int result = wove_test_main(argc, argv);
	assert(result == 42);
	assert(descriptor_failures == 1 && backing_failures == 5);
	assert(allocations > 0 && allocations == releases);
	for (size_t index = 0; index < sizeof live / sizeof live[0]; ++index) {
		assert(live[index] == NULL);
	}
	return result;
}

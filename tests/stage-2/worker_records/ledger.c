// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ledger.h"

enum scenario { collect, discard, joined, pack_frame, pack_storage, receipt_frame, start, joined_start, result_frame };
static enum scenario mode;
static const char *const scenarios[] = {
	"collect", "discard", "joined", "pack-frame", "pack-storage", "receipt-frame", "start", "joined-start", "result-frame"
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static void *allocations[16];
static size_t acquired;
static size_t released;
static bool active;
static int until_malloc_failure;
static bool fail_calloc;
static unsigned results;

static void require(bool condition) {
	if (!condition) { abort(); }
}

// Begin after the host adapter has already packed argv.
int32_t begin(void) {
	active = true;
	if (mode == discard) { return 1; }
	return mode == joined || mode == joined_start || mode == result_frame ? 2 : 0;
}

// Count only allocations starting with the job transport.
void before_spawn(void) {
	if (mode == pack_frame) { until_malloc_failure = 1; }
	if (mode == pack_storage) { fail_calloc = true; }
	if (mode == receipt_frame) { until_malloc_failure = 2; }
}

void before_result(void) {
	++results;
	if (mode == result_frame) { until_malloc_failure = 1; }
}

static void *track(void *pointer) {
	if (pointer == NULL || !active) { return pointer; }
	require(pthread_mutex_lock(&lock) == 0);
	for (size_t index = 0; index < sizeof allocations / sizeof allocations[0]; ++index) {
		if (allocations[index] == NULL) {
			allocations[index] = pointer;
			++acquired;
			require(pthread_mutex_unlock(&lock) == 0);
			return pointer;
		}
	}
	abort();
}

void *fixture_worker_malloc(size_t size) {
	if (until_malloc_failure > 0 && --until_malloc_failure == 0) { return NULL; }
	return track(malloc(size));
}

void *fixture_worker_calloc(size_t count, size_t size) {
	if (fail_calloc) {
		fail_calloc = false;
		return NULL;
	}
	return track(calloc(count, size));
}

void fixture_worker_free(void *pointer) {
	if (pointer != NULL && active) {
		require(pthread_mutex_lock(&lock) == 0);
		for (size_t index = 0; index < sizeof allocations / sizeof allocations[0]; ++index) {
			if (allocations[index] == pointer) {
				allocations[index] = NULL;
				++released;
				break;
			}
		}
		require(pthread_mutex_unlock(&lock) == 0);
	}
	free(pointer);
}

int fixture_worker_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*run)(void *), void *argument) {
	if (mode == start || mode == joined_start) { return EAGAIN; }
	return pthread_create(thread, attr, run, argument);
}

int fixture_worker_join(pthread_t thread, void **result) {
	return pthread_join(thread, result);
}

// Run even after a failed launch or result allocation reports a runtime trap.
static void check_ownership(void) {
	const size_t expected[] = {13, 13, 12, 4, 5, 6, 7, 6, 10};
	require(acquired == expected[mode] && released == acquired);
	require(results == (mode == collect || mode == discard || mode == joined || mode == result_frame));
}

#undef main
int wove_test_main(int argc, char **argv);

int main(int argc, char **argv) {
	require(argc == 1 || argc == 2);
	if (argc == 2) {
		size_t index = 0;
		while (index < sizeof scenarios / sizeof scenarios[0] && strcmp(argv[1], scenarios[index]) != 0) { ++index; }
		require(index < sizeof scenarios / sizeof scenarios[0]);
		mode = (enum scenario)index;
	}
	require(atexit(check_ownership) == 0);
	// The harness consumes the mode and the Wove fixture receives an empty user argv.
	return wove_test_main(1, argv);
}

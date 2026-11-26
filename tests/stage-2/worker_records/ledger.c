// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "ledger.h"

enum scenario {
	collect, discard, joined, pack_frame, pack_storage, receipt_frame, start, joined_start, result_frame,
	queue_collect, queue_discard, queue_allocate, queue_backing, queue_join, queue_discard_join
};
#ifdef RECORD_QUEUE_TEST
static enum scenario mode = queue_collect;
#else
static enum scenario mode = collect;
#endif
static const char *const scenarios[] = {
	"collect", "discard", "joined", "pack-frame", "pack-storage", "receipt-frame", "start", "joined-start", "result-frame",
	"queue-collect", "queue-discard", "queue-allocate", "queue-backing", "queue-join", "queue-discard-join"
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static void *allocations[64];
static size_t acquired;
static size_t released;
static bool active;
static _Thread_local int until_malloc_failure;
static _Thread_local bool fail_calloc;
static atomic_uint results;
static atomic_uint joins;

static void require(bool condition) {
	if (!condition) { abort(); }
}

// Begin after the host adapter has already packed argv.
int32_t begin(void) {
	active = true;
	if (mode == discard || mode == queue_discard || mode == queue_discard_join) { return 1; }
	return mode == joined || mode == joined_start || mode == result_frame ? 2 : 0;
}

// Arm the calling thread at the next launch or queue collection.
void before_spawn(void) {
	if (mode == pack_frame || mode == queue_allocate) { until_malloc_failure = 1; }
	if (mode == pack_storage || mode == queue_backing) { fail_calloc = true; }
	if (mode == receipt_frame) { until_malloc_failure = 2; }
}

void before_result(void) {
	atomic_fetch_add_explicit(&results, 1, memory_order_relaxed);
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

// Reap the thread and simulate the second join failing after it has published its owner.
int fixture_worker_join(pthread_t thread, void **result) {
	unsigned index = 1 + atomic_fetch_add_explicit(&joins, 1, memory_order_relaxed);
	int status = pthread_join(thread, result);
	if (status == 0 && index == 2 && (mode == queue_join || mode == queue_discard_join)) { return EINVAL; }
	return status;
}

// Run even after a failed launch or result allocation reports a runtime trap.
static void check_ownership(void) {
	const size_t expected[] = {
		[collect] = 13, [discard] = 13, [joined] = 12,
		[pack_frame] = 4, [pack_storage] = 5, [receipt_frame] = 6,
		[start] = 7, [joined_start] = 6, [result_frame] = 10,
		[queue_collect] = 45, [queue_discard] = 43, [queue_allocate] = 43,
		[queue_backing] = 44, [queue_join] = 45, [queue_discard_join] = 43
	};
	require(acquired == expected[mode] && released == acquired);
	unsigned completed = atomic_load_explicit(&results, memory_order_relaxed);
	// The queue must retire every accepted worker even when one join reports failure.
	if (mode >= queue_collect) {
		require(completed == 3);
		require(atomic_load_explicit(&joins, memory_order_relaxed) == 3);
	} else {
		require(completed == (unsigned)(mode == collect || mode == discard || mode == joined || mode == result_frame));
	}
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

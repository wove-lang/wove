/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "wove_runtime.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static void *allocations[64];
static size_t live_count;
static _Thread_local bool reject_malloc;
static _Thread_local bool reject_calloc;
static int expected_jobs;
static int reject_join;
static atomic_int attempts;
static atomic_int job_drops;
static atomic_int result_drops;

/* Check fixture invariants even when the native build disables assert. */
static void require(bool condition) {
	if (!condition) { abort(); }
}

/* Record each live runtime allocation across all participating threads. */
static void *remember(void *allocation) {
	if (allocation == NULL) { return NULL; }
	require(pthread_mutex_lock(&lock) == 0);
	require(live_count < sizeof allocations / sizeof allocations[0]);
	allocations[live_count++] = allocation;
	require(pthread_mutex_unlock(&lock) == 0);
	return allocation;
}

/* Reject only the caller's next descriptor allocation when requested. */
void *fixture_worker_malloc(size_t size) {
	/* Workers allocate normally even if the parent is about to fail. */
	if (reject_malloc) {
		reject_malloc = false;
		return NULL;
	}
	return remember(malloc(size));
}

/* Reject the caller's next queue growth or output backing allocation. */
void *fixture_worker_calloc(size_t count, size_t size) {
	/* This thread-local switch cannot intercept a concurrent handler. */
	if (reject_calloc) {
		reject_calloc = false;
		return NULL;
	}
	return remember(calloc(count, size));
}

/* Drop exactly one live tracked allocation. */
void fixture_worker_free(void *allocation) {
	if (allocation == NULL) { return; }
	require(pthread_mutex_lock(&lock) == 0);
	size_t index = 0;
	/* The search must find a currently live block once. */
	while (index < live_count && allocations[index] != allocation) {
		++index;
	}
	require(index < live_count);
	allocations[index] = allocations[--live_count];
	require(pthread_mutex_unlock(&lock) == 0);
	free(allocation);
}

/* Launch the native worker through the real thread API. */
int fixture_worker_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*run)(void *), void *argument) {
	return pthread_create(thread, attr, run, argument);
}

/* Release each queued worker as the parent attempts its join. */
int fixture_worker_join(pthread_t thread, void **result) {
	int attempt = 1 + atomic_fetch_add_explicit(&attempts, 1, memory_order_acq_rel);
	return attempt == reject_join ? EINVAL : pthread_join(thread, result);
}

/* Count each destroyed input element. */
static void drop_job(void *item) {
	(void)item;
	atomic_fetch_add_explicit(&job_drops, 1, memory_order_relaxed);
}

/* Count each destroyed result element. */
static void drop_result(void *item) {
	(void)item;
	atomic_fetch_add_explicit(&result_drops, 1, memory_order_relaxed);
}

/* Keep pending jobs live until retirement, then return differently typed results. */
static wvrt_array *handle(wvrt_array *job) {
	int32_t index = ((uint8_t *)job->wvrt_items)[0];
	/* A join failure cannot leave later jobs waiting indefinitely. */
	while (atomic_load_explicit(&attempts, memory_order_acquire) < index) {
		(void)sched_yield();
	}
	wvrt_array_drop(job);
	wvrt_array *result = wvrt_array_new_at_span(1, sizeof(int32_t), _Alignof(int32_t), 0, 0, 0);
	((int32_t *)result->wvrt_items)[0] = index;
	result->wvrt_drop_element = drop_result;
	return result;
}

/* Check that all frames, descriptors, and backing blocks retire even on exit. */
static void verify_cleanup(void) {
	require(atomic_load_explicit(&attempts, memory_order_relaxed) == expected_jobs);
	require(atomic_load_explicit(&job_drops, memory_order_relaxed) == expected_jobs);
	require(atomic_load_explicit(&result_drops, memory_order_relaxed) == expected_jobs);
	require(pthread_mutex_lock(&lock) == 0);
	require(live_count == 0);
	require(pthread_mutex_unlock(&lock) == 0);
}

/* Exercise empty, ordered, discarded, and partially failing receipt queues. */
int main(int argc, char **argv) {
	require(argc == 2);
	const char *mode = argv[1];
	const bool empty = strcmp(mode, "empty") == 0;
	const bool queue_alloc = strcmp(mode, "queue-allocate") == 0;
	const bool growth = strcmp(mode, "grow") == 0;
	const bool discard = strcmp(mode, "discard") == 0 || strcmp(mode, "discard-join") == 0;
	if (strcmp(mode, "join") == 0 || strcmp(mode, "discard-join") == 0) { reject_join = 2; }
	expected_jobs = empty || queue_alloc ? 0 : (growth ? 2 : 3);
	require(atexit(verify_cleanup) == 0);
	reject_malloc = queue_alloc;
	wvrt_array *queue = wvrt_worker_queue_at_span(3, 42, 49);
	/* Each job's input and result have a separate independently tracked owner. */
	for (int32_t index = 1; index <= expected_jobs; ++index) {
		wvrt_array *job = wvrt_array_new_u8_at_span(1, 0, 0, 0);
		((uint8_t *)job->wvrt_items)[0] = (uint8_t)index;
		job->wvrt_drop_element = drop_job;
		wvrt_worker_receipt *receipt = wvrt_worker_spawn_at_span(handle, job, 2, 10 + index, 37 + index);
		reject_calloc = growth && index == 2;
		wvrt_worker_queue_push_at_span(&queue, receipt, 4, 45, 58);
	}
	require(!growth);
	/* The scope-exit path never publishes any result to the caller. */
	if (discard) {
		wvrt_worker_queue_drop(queue);
		require(reject_join == 0);
		return 0;
	}
	reject_malloc = strcmp(mode, "allocate") == 0;
	reject_calloc = strcmp(mode, "allocate-backing") == 0;
	wvrt_array *results = wvrt_worker_join_all_at_span(&queue, 5, 60, 88);
	require(queue == NULL && results->wvrt_length == expected_jobs);
	require(atomic_load_explicit(&result_drops, memory_order_relaxed) == 0);
	wvrt_array **rows = results->wvrt_items;
	/* The array owns one exact original result at each submission index. */
	for (int32_t index = 0; index < expected_jobs; ++index) {
		require(((int32_t *)rows[index]->wvrt_items)[0] == index + 1);
	}
	wvrt_array_drop(results);
	return 0;
}

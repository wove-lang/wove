/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "wove_runtime.h"

static atomic_int allocations;
static atomic_int job_drops;
static atomic_int result_drops;
static atomic_int handled;
static atomic_bool entered;
static atomic_bool released;
static atomic_bool reserving;
static int create_calls;
static int join_calls;
static int fail_create;
static bool fail_malloc;
static int expected_jobs;
static int expected_results;
static pthread_t executor;

/* Keep the checks enabled in release builds. */
static void require(bool condition) {
	if (!condition) { abort(); }
}

/* Observe runtime allocation rollback and optionally fail the receipt descriptor. */
void *fixture_worker_malloc(size_t size) {
	if (fail_malloc) { return NULL; }
	void *allocation = malloc(size);
	if (allocation != NULL) { atomic_fetch_add(&allocations, 1); }
	return allocation;
}

/* Count the pool state, waiting ring, thread storage, and array backing blocks. */
void *fixture_worker_calloc(size_t count, size_t size) {
	void *allocation = calloc(count, size);
	if (allocation != NULL) { atomic_fetch_add(&allocations, 1); }
	return allocation;
}

/* Release each tracked block through the normal allocator. */
void fixture_worker_free(void *allocation) {
	if (allocation != NULL) { atomic_fetch_sub(&allocations, 1); }
	free(allocation);
}

/* Inject a partial startup failure through the same path as a native failure. */
int fixture_worker_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*run)(void *), void *argument) {
	if (++create_calls == fail_create) { return EAGAIN; }
	return pthread_create(thread, attr, run, argument);
}

/* Count physical thread joins separately from result collection. */
int fixture_worker_join(pthread_t thread, void **result) {
	++join_calls;
	return pthread_join(thread, result);
}

/* Count each destroyed input element. */
static void drop_job(void *item) {
	(void)item;
	atomic_fetch_add(&job_drops, 1);
}

/* Count each destroyed result element. */
static void drop_result(void *item) {
	(void)item;
	atomic_fetch_add(&result_drops, 1);
}

/* Hold the first running job until the producer fills the waiting ring. */
static wvrt_array *handle(wvrt_array *job) {
	uint8_t id = *(uint8_t *)job->wvrt_items;
	/* All accepted jobs must run in order on the same physical thread. */
	if (atomic_fetch_add(&handled, 1) == 0) {
		executor = pthread_self();
		atomic_store(&entered, true);
		/* Keep the running job outside the waiting capacity. */
		while (!atomic_load(&released)) { (void)sched_yield(); }
	}
	require(pthread_equal(executor, pthread_self()));
	require(atomic_load(&handled) == id);
	wvrt_array_drop(job);
	wvrt_array *result = wvrt_array_new_u8_at_span(1, 0, 0, 0);
	*(uint8_t *)result->wvrt_items = id;
	result->wvrt_drop_element = drop_result;
	return result;
}

/* Create independently tracked input and result owners. */
static wvrt_array *new_job(uint8_t id) {
	wvrt_array *job = wvrt_array_new_u8_at_span(1, 0, 0, 0);
	*(uint8_t *)job->wvrt_items = id;
	job->wvrt_drop_element = drop_job;
	return job;
}

/* Enqueue into a known live reservation. */
static wvrt_worker_receipt *submit(WvrtWorkerPermit *permit, uint8_t id) {
	require(permit != NULL);
	return wvrt_worker_spawn_on_at_span(handle, permit, new_job(id), 4, 61, 82);
}

/* Collect the same receipt type returned by a dedicated worker. */
static void collect(wvrt_worker_receipt *receipt, uint8_t id) {
	wvrt_array *result = wvrt_worker_join_at_span(&receipt, 0, 0, 0);
	require(receipt == NULL && *(uint8_t *)result->wvrt_items == id);
	wvrt_array_drop(result);
}

/* The scheduler owns no storage after every permit and receipt retires. */
static void verify_cleanup(void) {
	require(atomic_load(&allocations) == 0);
	require(atomic_load(&job_drops) == expected_jobs);
	require(atomic_load(&result_drops) == expected_results);
}

/* Hand off producer admission to a waiter while the test releases the active job. */
static void *wait_for_slot(void *pool) {
	atomic_store(&reserving, true);
	return wvrt_worker_pool_reserve_wait(pool);
}

/* Mix one waiting frame with one unused permit and drain accepted jobs. */
static void check_draining(void) {
	expected_jobs = 3;
	expected_results = 3;
	WvrtWorkerPool *pool = wvrt_worker_pool_open(1, 2);
	require(pool != NULL);
	wvrt_worker_receipt *first = submit(wvrt_worker_pool_reserve(pool), 1);
	/* Wait until the first job is running and no longer counts against the bound. */
	while (!atomic_load(&entered)) { (void)sched_yield(); }
	WvrtWorkerPermit *unused = wvrt_worker_pool_reserve(pool);
	WvrtWorkerPermit *second = wvrt_worker_pool_reserve(pool);
	require(unused != NULL && second != NULL);
	require(wvrt_worker_pool_reserve_wait(pool) == NULL);
	require(!wvrt_worker_pool_close(pool));
	wvrt_worker_receipt *waiting = submit(second, 2);
	require(wvrt_worker_pool_reserve(pool) == NULL);
	pthread_t waiter;
	require(pthread_create(&waiter, NULL, wait_for_slot, pool) == 0);
	/* A running worker can now free a queued slot independently of the producer. */
	while (!atomic_load(&reserving)) { (void)sched_yield(); }
	atomic_store(&released, true);
	void *permit;
	require(pthread_join(waiter, &permit) == 0 && permit != NULL);
	wvrt_worker_permit_drop(unused);
	wvrt_worker_receipt *last = submit(permit, 3);
	require(wvrt_worker_pool_close(pool));
	require(atomic_load(&handled) == 3);
	require(wvrt_worker_pool_close(pool));
	require(wvrt_worker_pool_reserve_wait(pool) == NULL);
	wvrt_worker_pool_drop(pool);
	require(create_calls == 1 && join_calls == 1);
	collect(waiting, 2);
	collect(first, 1);
	collect(last, 3);
}

/* Leaving pool scope preserves reservations and receipts without reopening admission. */
static void check_retirement(bool submit_last) {
	expected_jobs = submit_last ? 2 : 1;
	expected_results = expected_jobs;
	WvrtWorkerPool *pool = wvrt_worker_pool_open(1, 2);
	require(pool != NULL);
	atomic_store(&released, true);
	wvrt_worker_receipt *first = submit(wvrt_worker_pool_reserve(pool), 1);
	WvrtWorkerPermit *last = wvrt_worker_pool_reserve(pool);
	require(last != NULL);
	wvrt_worker_pool_drop(pool);
	require(join_calls == 0);
	/* Resolving the final reservation joins the workers after the accepted jobs drain. */
	if (submit_last) {
		collect(submit(last, 2), 2);
	} else {
		wvrt_worker_permit_drop(last);
	}
	require(join_calls == 1 && atomic_load(&handled) == expected_jobs);
	collect(first, 1);
}

/* Check partial startup and both ownership cleanup paths after failed submission. */
int main(int argc, char **argv) {
	require(argc == 2 && atexit(verify_cleanup) == 0);
	const char *mode = argv[1];
	/* An unavailable pool and its empty reservations can leave lexical scope. */
	if (strcmp(mode, "startup") == 0) {
		require(wvrt_worker_pool_open(0, 2) == NULL);
		require(wvrt_worker_pool_open(2, -1) == NULL);
		wvrt_worker_pool_drop(NULL);
		wvrt_worker_permit_drop(NULL);
		require(wvrt_worker_pool_reserve(NULL) == NULL && !wvrt_worker_pool_close(NULL));
		fail_create = 2;
		require(wvrt_worker_pool_open(3, 2) == NULL);
		require(create_calls == 2 && join_calls == 1);
		return 0;
	}
	/* These paths must trap after dropping the input exactly once. */
	if (strcmp(mode, "missing") == 0 || strcmp(mode, "submission") == 0) {
		expected_jobs = 1;
		wvrt_array *job = new_job(1);
		WvrtWorkerPermit *permit = NULL;
		/* A failed submit can be the event that finally releases a retired pool. */
		if (strcmp(mode, "submission") == 0) {
			WvrtWorkerPool *pool = wvrt_worker_pool_open(1, 1);
			require(pool != NULL);
			permit = wvrt_worker_pool_reserve(pool);
			require(permit != NULL);
			wvrt_worker_pool_drop(pool);
			fail_malloc = true;
		}
		(void)wvrt_worker_spawn_on_at_span(handle, permit, job, 4, 61, 82);
		abort();
	}
	/* Each manifest case starts with a fresh producer and no pending work. */
	if (strcmp(mode, "drain") == 0) {
		check_draining();
	} else {
		require(strcmp(mode, "drop-submit") == 0 || strcmp(mode, "drop-unused") == 0);
		check_retirement(strcmp(mode, "drop-submit") == 0);
	}
	return 0;
}

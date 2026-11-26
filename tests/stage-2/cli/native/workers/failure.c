/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "wove_runtime.h"

static bool reject_create;
static bool reject_join;
static pthread_t caller;
static atomic_bool join_attempted;
static atomic_int jobs;
static atomic_int job_drops;
static atomic_int result_drops;

/* Abort independently of whether the configured build disables assertions. */
static void require(bool condition) {
	if (!condition) {
		abort();
	}
}

/* Make creation fail before it has accepted the job. */
int fixture_worker_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*run)(void *), void *argument) {
	if (reject_create) {
		return EAGAIN;
	}
	return pthread_create(thread, attr, run, argument);
}

/* Make the first join fail while the handler still owns its input. */
int fixture_worker_join(pthread_t thread, void **result) {
	if (reject_join) {
		atomic_store_explicit(&join_attempted, true, memory_order_release);
		return EINVAL;
	}
	return pthread_join(thread, result);
}

/* Count each destroyed input element. */
static void record_job_drop(void *item) {
	(void)item;
	atomic_fetch_add_explicit(&job_drops, 1, memory_order_relaxed);
}

/* Count each destroyed returned element. */
static void record_result_drop(void *item) {
	(void)item;
	atomic_fetch_add_explicit(&result_drops, 1, memory_order_relaxed);
}

/* Run only on the new thread and produce a different element representation. */
static wvrt_array *handle(wvrt_array *job) {
	require(!pthread_equal(caller, pthread_self()));
	atomic_fetch_add_explicit(&jobs, 1, memory_order_relaxed);
	// The failed-join scenario proves that the runtime waits before dropping the result.
	while (reject_join && !atomic_load_explicit(&join_attempted, memory_order_acquire)) {
		(void)sched_yield();
	}
	wvrt_array_drop(job);
	wvrt_array *result = wvrt_array_new_at_span(2, sizeof(int32_t), _Alignof(int32_t), 0, 0, 0);
	result->wvrt_drop_element = record_result_drop;
	return result;
}

/* Also run after the runtime reports an error through exit(). */
static void check_cleanups(void) {
	require(atomic_load_explicit(&job_drops, memory_order_relaxed) == 3);
	require(atomic_load_explicit(&jobs, memory_order_relaxed) == (reject_create ? 0 : 1));
	require(atomic_load_explicit(&result_drops, memory_order_relaxed) == (reject_create ? 0 : 2));
}

/* Exercise successful ownership delivery and both injected thread API failures. */
int main(int argc, char **argv) {
	require(argc == 2);
	reject_create = strcmp(argv[1], "create") == 0;
	reject_join = strcmp(argv[1], "join") == 0;
	require(reject_create || reject_join || strcmp(argv[1], "success") == 0);
	caller = pthread_self();
	require(atexit(check_cleanups) == 0);
	wvrt_array *job = wvrt_array_new_u8_at_span(3, 0, 0, 0);
	job->wvrt_drop_element = record_job_drop;
	wvrt_array *result = wvrt_worker_spawn_join_at_span(handle, job, 2, 10, 37);
	require(!reject_create && !reject_join);
	require(result->wvrt_length == 2 && result->wvrt_element_size == sizeof(int32_t));
	require(atomic_load_explicit(&result_drops, memory_order_relaxed) == 0);
	wvrt_array_drop(result);
	return 0;
}

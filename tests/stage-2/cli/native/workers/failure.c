/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "wove_runtime.h"

static bool submitted;
static bool reject_allocation;
static bool observe_receipt_allocation;
static void *receipt_frame;
static int receipt_drops;
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

/* Isolate the next runtime allocation, which belongs to the new receipt. */
void *fixture_worker_malloc(size_t size) {
	/* Only submission itself can consume the immediate observation flag. */
	if (observe_receipt_allocation) {
		observe_receipt_allocation = false;
		if (reject_allocation) { return NULL; }
		receipt_frame = malloc(size);
		return receipt_frame;
	}
	return malloc(size);
}

/* Keep the shared C build recipe's backing storage allocation unchanged here. */
void *fixture_worker_calloc(size_t count, size_t size) {
	return calloc(count, size);
}

/* Count retirement of the receipt separately from its input and result. */
void fixture_worker_free(void *pointer) {
	/* The worker's other allocations are released independently. */
	if (pointer != NULL && pointer == receipt_frame) {
		receipt_frame = NULL;
		++receipt_drops;
	}
	free(pointer);
}

/* Make creation fail before it has accepted the job. */
int fixture_worker_create(pthread_t *thread, const pthread_attr_t *attr,
	void *(*run)(void *), void *argument) {
	if (reject_create) {
		return EAGAIN;
	}
	return pthread_create(thread, attr, run, argument);
}

/* Release the worker when the caller attempts a join and optionally force failure. */
int fixture_worker_join(pthread_t thread, void **result) {
	atomic_store_explicit(&join_attempted, true, memory_order_release);
	return reject_join ? EINVAL : pthread_join(thread, result);
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
	// In gated cases the handler cannot finish before explicit or implicit collection begins.
	while ((submitted || reject_join) && !atomic_load_explicit(&join_attempted, memory_order_acquire)) {
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
	const bool launched = !reject_create && !reject_allocation;
	require(atomic_load_explicit(&jobs, memory_order_relaxed) == (launched ? 1 : 0));
	require(atomic_load_explicit(&result_drops, memory_order_relaxed) == (launched ? 2 : 0));
	require(receipt_frame == NULL);
	require(receipt_drops == (submitted && !reject_allocation ? 1 : 0));
}

/* Confirm that a received array is intact and transfers cleanup to the caller. */
static void drop_delivered_result(wvrt_array *result) {
	require(result->wvrt_length == 2 && result->wvrt_element_size == sizeof(int32_t));
	require(atomic_load_explicit(&result_drops, memory_order_relaxed) == 0);
	wvrt_array_drop(result);
}

/* Keep receipt moves, explicit joins and lexical cleanup on the same ABI as emitted C. */
static void check_receipt(wvrt_array *job, const char *mode) {
	observe_receipt_allocation = true;
	wvrt_worker_receipt *receipt = wvrt_worker_spawn_at_span(handle, job, 2, 10, 37);
	require(!reject_create && !reject_allocation);
	/* Scope exit joins at the original submission site. */
	if (strcmp(mode, "discard") == 0 || strcmp(mode, "discard-join") == 0) {
		wvrt_worker_receipt_drop(receipt);
		require(!reject_join);
		return;
	}
	wvrt_worker_receipt *moved = wvrt_worker_receipt_take(&receipt);
	require(receipt == NULL);
	wvrt_worker_receipt_drop(receipt);
	wvrt_array *result = wvrt_worker_join_at_span(&moved, 3, 50, 74);
	require(!reject_join && moved == NULL);
	drop_delivered_result(result);
	/* An extra join of the cleared slot must report its own source span. */
	if (strcmp(mode, "empty") == 0) {
		(void)wvrt_worker_join_at_span(&moved, 4, 90, 112);
	}
}

/* Exercise joined and submitted workers through independently invoked host cases. */
int main(int argc, char **argv) {
	require(argc == 2 || argc == 3);
	submitted = argc == 3 && strcmp(argv[1], "receipt") == 0;
	require(argc == 2 || submitted);
	const char *mode = argv[argc - 1];
	reject_allocation = submitted && strcmp(mode, "allocate") == 0;
	reject_create = strcmp(mode, "create") == 0;
	reject_join = strcmp(mode, "join") == 0 || (submitted && strcmp(mode, "discard-join") == 0);
	require(reject_allocation || reject_create || reject_join || strcmp(mode, "success") == 0 ||
		(submitted && (strcmp(mode, "discard") == 0 || strcmp(mode, "empty") == 0)));
	caller = pthread_self();
	require(atexit(check_cleanups) == 0);
	wvrt_array *job = wvrt_array_new_u8_at_span(3, 0, 0, 0);
	job->wvrt_drop_element = record_job_drop;
	/* The original joined path shares launch and join handling with the new receipt path. */
	if (submitted) {
		check_receipt(job, mode);
		return 0;
	}
	wvrt_array *result = wvrt_worker_spawn_join_at_span(handle, job, 2, 10, 37);
	require(!reject_create && !reject_join);
	drop_delivered_result(result);
	return 0;
}

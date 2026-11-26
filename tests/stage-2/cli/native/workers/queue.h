/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#ifndef WOVE_TEST_RECEIPT_QUEUE_H
#define WOVE_TEST_RECEIPT_QUEUE_H

#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

static pthread_key_t queue_completed_key;
static pthread_once_t queue_completed_once = PTHREAD_ONCE_INIT;
static _Atomic int32_t queue_started;
static _Atomic int32_t queue_released;
static _Atomic int32_t queue_completed;

/* Record completion from the POSIX thread-exit destructor. */
static void queue_record_completion(void *value) {
	atomic_fetch_or_explicit(&queue_completed, (int32_t)(uintptr_t)value, memory_order_release);
}

/* Install a single completion hook shared by the fixture's workers. */
static void queue_init_completion(void) {
	if (pthread_key_create(&queue_completed_key, queue_record_completion) != 0) { abort(); }
}

/* Wait for all selected workers at a synchronization gate. */
static void queue_wait(_Atomic int32_t *gate, int32_t mask) {
	/* The native runner times out if an expected worker cannot reach this gate. */
	while ((atomic_load_explicit(gate, memory_order_acquire) & mask) != mask) {
		(void)sched_yield();
	}
}

/* Register the job and hold it until the caller releases its bit. */
static inline int32_t queue_enter(int32_t mask) {
	/* The completion hook runs after this handler's result has been published. */
	if (pthread_once(&queue_completed_once, queue_init_completion) != 0 ||
			pthread_setspecific(queue_completed_key, (void *)(uintptr_t)mask) != 0) {
		abort();
	}
	atomic_fetch_or_explicit(&queue_started, mask, memory_order_release);
	queue_wait(&queue_released, mask);
	return 0;
}

/* Force the second submitted worker to exit before the first can return. */
static inline int32_t queue_finish_second_first(void) {
	queue_wait(&queue_started, 3);
	atomic_fetch_or_explicit(&queue_released, 2, memory_order_release);
	queue_wait(&queue_completed, 2);
	int32_t completed = atomic_load_explicit(&queue_completed, memory_order_acquire);
	atomic_fetch_or_explicit(&queue_released, 1, memory_order_release);
	return completed;
}

/* Let an unclaimed worker finish when the function owning its queue exits. */
static inline int32_t queue_release_unclaimed(void) {
	atomic_fetch_or_explicit(&queue_released, 4, memory_order_release);
	return 0;
}

/* Report all completed handlers after their queues have been retired. */
static inline int32_t queue_finished(void) {
	return atomic_load_explicit(&queue_completed, memory_order_acquire);
}

#endif

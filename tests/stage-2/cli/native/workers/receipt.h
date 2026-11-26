/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#ifndef WOVE_TEST_WORKER_RECEIPT_H
#define WOVE_TEST_WORKER_RECEIPT_H

#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>

static _Atomic int32_t receipt_started;
static _Atomic int32_t receipt_released;
static _Atomic int32_t receipt_finished;

/* Wait until every job selected by the mask reaches the supplied gate. */
static inline void receipt_wait(_Atomic int32_t *gate, int32_t mask) {
	/* The manifest runner bounds the test if an expected thread never arrives. */
	while ((atomic_load_explicit(gate, memory_order_acquire) & mask) != mask) {
		(void)sched_yield();
	}
}

/* Tell the caller the job started, then wait for permission to return. */
static inline int32_t receipt_enter(int32_t mask) {
	atomic_fetch_or_explicit(&receipt_started, mask, memory_order_release);
	receipt_wait(&receipt_released, mask);
	return 0;
}

/* Allow the caller to prove progress before invoking either join. */
static inline int32_t receipt_wait_started(int32_t mask) {
	receipt_wait(&receipt_started, mask);
	return 0;
}

/* Release the selected workers before explicit or lexical joins. */
static inline int32_t receipt_release(int32_t mask) {
	atomic_fetch_or_explicit(&receipt_released, mask, memory_order_release);
	return 0;
}

/* Mark the last operation of each Wove handler. */
static inline int32_t receipt_finish(int32_t mask) {
	atomic_fetch_or_explicit(&receipt_finished, mask, memory_order_release);
	return 0;
}

/* Report which handlers had reached completion when the caller resumed. */
static inline int32_t receipt_completed(void) {
	return atomic_load_explicit(&receipt_finished, memory_order_acquire);
}

#endif

/* Copyright (c) 2026 wove contributors */
/* SPDX-License-Identifier: MIT OR Apache-2.0 */

#ifndef WOVE_TEST_WORKER_THREAD_H
#define WOVE_TEST_WORKER_THREAD_H

/* Each newly created thread sees its own zero-initialized marker. */
static inline int worker_first_visit(void) {
	static _Thread_local int visited;
	int first = !visited;
	visited = 1;
	return first;
}

#endif

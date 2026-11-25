// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_SCALAR_REFS_H
#define WOVE_TEST_NATIVE_SCALAR_REFS_H

#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

// Start a child that exits with a known status for the real waitpid binding.
static inline pid_t start_child(int32_t status) {
	pid_t child = fork();
	if (child == 0) { _exit(status); }
	return child;
}

// Write a distinct value through the declared pid_t pointer.
static inline int32_t fill_pid(pid_t *status) {
	*status = (pid_t)12345;
	return 0;
}

// Replace a scalar with a known value so argument order is observable.
static inline int32_t update_status(int32_t *status) {
	*status = 99;
	return 17;
}

// Write only to the named output, using the already evaluated value.
static inline int32_t capture_status(int32_t before, int32_t *status) {
	*status = 99;
	return before;
}

#endif

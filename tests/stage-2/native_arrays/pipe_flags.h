// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_PIPE_FLAGS_H
#define WOVE_TEST_PIPE_FLAGS_H

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>

// Observe FD_CLOEXEC using the real header's descriptor query.
static inline bool has_close_on_exec(int32_t descriptor) {
	const int flags = fcntl(descriptor, F_GETFD);
	return flags >= 0 && (flags & FD_CLOEXEC) == FD_CLOEXEC;
}

#endif

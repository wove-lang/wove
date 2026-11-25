// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TRACKED_PATH_H
#define WOVE_TEST_TRACKED_PATH_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *active_path;
static int32_t release_count;

// Return absent, present empty, or present non-ASCII text in distinct cases.
static inline char *acquire_path(int32_t mode) {
	if (mode == 0) { return NULL; }
	if (active_path != NULL) { abort(); }
	active_path = malloc(3);
	if (active_path == NULL) { abort(); }
	memcpy(active_path, mode == 1 ? "\0\0" : "\xc3\xa9", 3);
	return active_path;
}

// Poison and free the exact C allocation before Wove inspects its copy.
static inline void release_path(char *path) {
	if (path != active_path || path == NULL) { abort(); }
	++release_count;
	memset(path, 0xff, 3);
	free(path);
	active_path = NULL;
}

// Let the Wove fixture count native release calls independently of Wove cleanup.
static inline int32_t releases(void) {
	return release_count;
}

#endif

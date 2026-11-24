// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_C_OWNERS_H
#define WOVE_TEST_C_OWNERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Use native spellings that collide with the generated local at the first two prefixes.
typedef struct {
	int32_t key;
} wgl000003iouter;

static wgl000003iouter wove_test_resources[] = {{1}, {2}, {3}, {4}};
static int32_t wove_test_drops[5];
static int32_t wove_test_last_drop;

// Return a distinct native pointer for every nonzero key.
static inline wgl000003iouter *wove_test_acquire(int32_t key) {
	return key == 0 ? NULL : &wove_test_resources[key - 1];
}

// Check that this acquisition receives a terminated C string before returning the handle.
static inline wgl000003iouter *wove_test_acquire_text(const char *text) {
	return text && text[0] == '\0' ? &wove_test_resources[3] : NULL;
}

// Observe exactly the pointer returned by the factory and catch an erroneous null drop.
static inline int32_t wwgl000003iouter(wgl000003iouter *resource) {
	int32_t key = resource ? resource->key : 0;
	++wove_test_drops[key];
	wove_test_last_drop = key;
	return -1;
}

// Report the current count for one resource so Wove can check the lifetime boundaries.
static inline bool wove_test_dropped(int32_t key, int32_t count) {
	return wove_test_drops[key] == count;
}

// Verify reverse cleanup order and detect any duplicate or null destruction.
static inline int32_t wove_test_finish(void) {
	return wove_test_drops[0] == 0 && wove_test_drops[1] == 1 && wove_test_drops[2] == 1 &&
		wove_test_drops[3] == 1 && wove_test_drops[4] == 1 && wove_test_last_drop == 3 ? 42 : 1;
}

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_SEED_BRIDGE_STORAGE_H
#define WOVE_SEED_BRIDGE_STORAGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct fixture_storage {
	int32_t marker;
};

static int32_t fixture_drops;

// An unsuccessful initializer leaves the object inactive even if C touched its bytes.
static inline int32_t fixture_prepare(struct fixture_storage *object, int32_t status) {
	object->marker = status == 0 ? 42 : -1;
	return status;
}

// Nullable loans expose the initialized object or an explicit null.
static inline int32_t fixture_inspect(const struct fixture_storage *object) {
	return object == NULL ? 0 : object->marker;
}

// Distinguish a correct release from accidental destruction of inactive storage.
static inline int32_t fixture_destroy(struct fixture_storage *object) {
	fixture_drops += object->marker == 42 ? 1 : 100;
	object->marker = -1;
	return 0;
}

typedef struct fixture_storage *fixture_handle;
static int32_t fixture_closes;

// A zero value represents a failed acquisition with no destructor obligation.
static inline fixture_handle fixture_acquire(int32_t marker) {
	// Return a null pointer on failure, including when allocation fails.
	if (marker == 0) {
		return NULL;
	}
	fixture_handle handle = malloc(sizeof *handle);
	// The returned handle alone owns its allocation.
	if (handle != NULL) {
		handle->marker = marker;
	}
	return handle;
}

// Mutably borrow the native handle without transferring it.
static inline int32_t fixture_update(fixture_handle handle) {
	return ++handle->marker;
}

// Return an owner to C explicitly or through the enclosing Wove scope.
static inline int32_t fixture_close(fixture_handle handle) {
	int32_t marker = handle->marker;
	free(handle);
	++fixture_closes;
	return marker;
}

#endif

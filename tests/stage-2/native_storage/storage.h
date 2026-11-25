// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_STORAGE_H
#define WOVE_TEST_NATIVE_STORAGE_H

#include <stdint.h>

typedef struct {
	_Alignas(64) int32_t value;
} fixture_storage;

static const fixture_storage *last_initialized;
static int32_t destructor_calls;

// Check allocation alignment and initial contents before trying the operation.
static inline int32_t fixture_init(fixture_storage *storage, _Bool succeed) {
	if ((uintptr_t)storage % _Alignof(fixture_storage) != 0 || storage->value != 0) { return 91; }
	if (!succeed) { return 17; }
	storage->value = 73;
	last_initialized = storage;
	return 0;
}

// Require all access to use the same address supplied at initialization.
static inline int32_t fixture_read(const fixture_storage *storage) {
	return storage == last_initialized ? storage->value : 92;
}

// Reject an inactive or moved address and count actual destructor calls.
static inline int32_t fixture_drop(fixture_storage *storage) {
	++destructor_calls;
	if (storage != last_initialized || storage->value != 73) { return 93; }
	storage->value = 0;
	last_initialized = 0;
	return 23;
}

// Report destructor activity after a Wove scope has released its owners.
static inline int32_t fixture_drops(void) {
	return destructor_calls;
}

// Serve as a checked shared and mutable pair of object parameters.
static inline int32_t fixture_pair(fixture_storage *left, const fixture_storage *right) {
	left->value = right->value;
	return 0;
}

#endif

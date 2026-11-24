// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_C_HANDLES_H
#define WOVE_TEST_C_HANDLES_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct { int32_t value; } WoveTestHandle;
typedef WoveTestHandle WoveTestBootstrapHandle;
static const WoveTestHandle wove_test_file = {42};
static int32_t wove_test_calls;
static int32_t wove_test_handle_drops;

// The owner stores this exact address or a missing handle.
static inline const WoveTestHandle *wove_test_open_handle(bool present) {
	return present ? &wove_test_file : NULL;
}

// A distinct Wove resource can have the same C representation.
static inline const WoveTestHandle *wove_test_open_other(void) {
	return &wove_test_file;
}

// Record destruction of the exact acquired handle.
static inline int32_t wove_test_drop_handle(const WoveTestHandle *file) {
	if (file != &wove_test_file) { abort(); }
	return ++wove_test_handle_drops;
}

// Abort on a null or changed pointer, premature cleanup, or repeated step.
static inline bool wove_test_observe_handle(const WoveTestHandle *file, int32_t step) {
	// Pointer identity is the first check, before any access to the resource.
	if (file != &wove_test_file || wove_test_handle_drops != 0 || step != wove_test_calls) {
		abort();
	}
	++wove_test_calls;
	return file->value == 42;
}

// Observe one destructor after both loans and the owner's lexical scope end.
static inline int32_t wove_test_finish_handles(void) {
	return wove_test_calls == 2 && wove_test_handle_drops == 1 ? 42 : 1;
}

#endif

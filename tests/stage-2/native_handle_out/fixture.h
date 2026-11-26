// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_NATIVE_HANDLE_OUT_FIXTURE_H
#define WOVE_NATIVE_HANDLE_OUT_FIXTURE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct fixture_handle {
	int32_t value;
} fixture_handle;

static int32_t opens;
static int32_t drops;
static int32_t live;

// Model null failure, nonnull failure, and success with a single native signature.
static inline int32_t fixture_open(fixture_handle **output, int32_t scenario) {
	++opens;
	if (*output != NULL) { abort(); }
	// A failure can leave a null output untouched.
	if (scenario == 0) { return -7; }
	fixture_handle *handle = malloc(sizeof *handle);
	if (handle == NULL) { return -9; }
	++live;
	handle->value = 42;
	*output = handle;
	return scenario == 1 ? 8 : 0;
}

// Check projected argument types before publishing the successful native handle.
static inline int32_t fixture_open_rows(char *const *rows, fixture_handle **output, const char *optional) {
	if (rows[0] == NULL || strcmp(rows[0], "A") != 0 || rows[1] != NULL || optional != NULL) { return -11; }
	return fixture_open(output, 2);
}

// A void pointer would accept the call through implicit conversion.
static inline int32_t fixture_wrong(void *output) {
	return output != NULL;
}

// Check the pointer received by an ordinary shared handle borrow.
static inline int32_t fixture_read(fixture_handle *handle) {
	return handle->value;
}

// Supply an output and a separate borrowed handle to the same call.
static inline int32_t fixture_dual(fixture_handle **output, fixture_handle *other) {
	return fixture_open(output, fixture_read(other));
}

// Account for every explicit or lexical destruction.
static inline int32_t fixture_drop(fixture_handle *handle) {
	++drops;
	--live;
	free(handle);
	return 0;
}

// Count every native open attempt.
static inline int32_t fixture_opens(void) { return opens; }

// Count native destructor invocations after the exercised scopes end.
static inline int32_t fixture_drops(void) { return drops; }

// Confirm that each published handle was released.
static inline int32_t fixture_live(void) { return live; }

// Show whether a later operand executes when an earlier output guard fails.
static inline int32_t fixture_later(void) {
	puts("later argument evaluated");
	return 2;
}

// A checked bridge must invoke the declared function even when a macro shares its name.
#define fixture_open(...) (-123)

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_SEED_BRIDGE_SNAPSHOT_H
#define WOVE_SEED_BRIDGE_SNAPSHOT_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint32_t snapshot_releases;

// Return allocated text only in allocation mode, and check that the first result was released.
static inline char *fixture_snapshot(const char *input, char *output) {
	// A second native call observes one release of the first result.
	if (output != NULL || (snapshot_releases == 1 && strcmp(input, "missing") == 0)) {
		return NULL;
	}
	const size_t size = strlen(input) + 1;
	char *copy = malloc(size);
	return copy == NULL ? NULL : memcpy(copy, input, size);
}

// Release the allocated result through a typed project destructor.
static inline void fixture_release(char *text) {
	++snapshot_releases;
	free(text);
}

// Return a suffix that is valid only while the converted argument is alive.
static inline const char *fixture_suffix(const char *input) {
	return input + 1;
}

// Mutate one byte of a checked C string without writing outside its view.
static inline int32_t fixture_change(char *input) {
	input[0] = 'z';
	return 0;
}

#endif

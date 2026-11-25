// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "wove_runtime.h"

static void *scratch;
static unsigned releases;

// Record the scratch allocation while using the real allocator.
void *tracked_malloc(size_t size) {
	scratch = malloc(size);
	return scratch;
}

// Forward the other allocation hook required by the tracked runtime.
void *tracked_calloc(size_t count, size_t size) {
	return calloc(count, size);
}

// Simulate a free implementation that changes errno after releasing storage.
void tracked_free(void *storage) {
	// The one prepared C string backing block must be freed exactly once.
	if (storage != scratch || scratch == NULL) {
		fputs("unexpected C string release\n", stderr);
		exit(EXIT_FAILURE);
	}
	scratch = NULL;
	++releases;
	free(storage);
	errno = EINVAL;
}

// A caller must observe the native call's errno after its temporary storage is freed.
int main(void) {
	wvrt_cstr_argument argument = {
		.wvrt_view = { "x", 1, sizeof(uint8_t) },
		.wvrt_checked = true,
	};
	char *storage = wvrt_cstr_prepare(&argument, 1);
	errno = ENOENT;
	wvrt_cstr_drop(storage);
	// Require both the original error value and the release.
	if (errno != ENOENT || releases != 1) {
		fputs("C string cleanup must release scratch and preserve errno\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

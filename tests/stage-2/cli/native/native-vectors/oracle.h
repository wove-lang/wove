// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <glob.h>
#include <stddef.h>
#include <stdint.h>
// Fill a real glob result with two sorted paths, following its reserved slots.
static inline int fixture_start(glob_t *out) {
	return glob("rows/*.txt", GLOB_DOOFFS, NULL, out);
}
// Grow the original vector after the scoped Wove view has ended.
static inline int fixture_append(glob_t *out) {
	return glob("rows/*.new", GLOB_APPEND | GLOB_DOOFFS, NULL, out);
}
// Check identity against the actual SDK pointer table and reserved prefix.
static inline _Bool fixture_original(const glob_t *out, const void *data, int32_t length) {
	return out->gl_offs == 2 && length == 10 && data == out->gl_pathv[2] &&
		out->gl_pathv[0] == NULL && out->gl_pathv[1] == NULL;
}

static int glob_drops;

// Track the native destructor while keeping the exact SDK representation.
static inline void fixture_drop(glob_t *out) {
	globfree(out);
	++glob_drops;
}

// Report how many managed allocations were destroyed.
static inline int fixture_drops(void) {
	return glob_drops;
}

// Exercise a valid result with zero matching paths and a reserved prefix.
static inline int fixture_empty(glob_t *out) {
	return glob("missing/*.txt", GLOB_DOOFFS, NULL, out) == GLOB_NOMATCH ? 17 : 18;
}

// The first matched alternative stays live on Darwin. glibc retains the
// earlier vector on a failed append, so both branches produce a partial result.
static inline int fixture_partial(glob_t *out) {
#ifdef __APPLE__
	int status = glob("{rows/*.txt,loop/*}", GLOB_BRACE | GLOB_ERR, NULL, out);
#else
	int status = glob("rows/*.txt", 0, NULL, out);
	// A failed first walk has no prior result to preserve.
	if (status != 0) {
		return 19;
	}
	status = glob("loop/*", GLOB_APPEND | GLOB_ERR, NULL, out);
#endif
	return status == GLOB_ABORTED ? 41 : 40;
}

#if defined(__linux__) && defined(_LARGEFILE64_SOURCE)
// Exercise the separately named large-file result with its own native libc call.
static inline int fixture_start64(glob64_t *out) {
	return glob64("rows/*.txt", GLOB_DOOFFS, NULL, out);
}
#endif

#ifdef FIXTURE_OPTIONAL_OWNER
typedef glob_t FixtureOptionalGlob;
#endif

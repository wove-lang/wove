// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>

#undef fopen
#undef fclose
#undef main

int wove_stream_main(void);

static FILE *active;
static int opened;
static int missing;
static int closed;
static int broken;

// Call real libc and remember the one stream expected to be live in this fixture.
FILE *wove_test_fopen(const char *path, const char *mode) {
	// A new acquisition requires the prior stream to have been closed.
	if (active != NULL) {
		broken = 1;
		return NULL;
	}
	active = fopen(path, mode);
	// Track missing results separately from live streams.
	if (active == NULL) {
		++missing;
	} else {
		++opened;
	}
	return active;
}

// Close the actual stream once, then simulate a failure status on one consuming path.
int wove_test_fclose(FILE *stream) {
	// Reject unexpected cleanup without invoking libc on an invalid stream.
	if (stream == NULL || stream != active) {
		broken = 1;
		return EOF;
	}
	active = NULL;
	++closed;
	int status = fclose(stream);
	if (status != 0) { broken = 1; }
	return closed == 3 ? EOF : status;
}

// Check one missing stream and exactly one close for every successful acquisition.
int main(void) {
	int result = wove_stream_main();
	if (result != 42) { return result; }
	return !broken && active == NULL && missing == 1 && opened == 4 && closed == 4 ? 42 : 6;
}

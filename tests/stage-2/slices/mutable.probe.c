// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <stdio.h>
#include <string.h>

#undef main
int wove_emitted_main(int argc, char **argv);
uint8_t wgf2udp8pjforward_write(wvrt_mut_slice view, int32_t index);

// Observe the same writable storage before and after a Wove reborrow.
int main(int argc, char **argv) {
	uint8_t data[] = {9, 5, 7};
	const wvrt_mut_slice full = {data, 3, sizeof *data};
	const wvrt_mut_slice empty = {NULL, 0, sizeof *data};
	// Each invalid store must terminate in the bounds check.
	if (argc == 2) {
		if (strcmp(argv[1], "negative") == 0) { (void)wgf2udp8pjforward_write(full, -1); }
		if (strcmp(argv[1], "end") == 0) { (void)wgf2udp8pjforward_write(full, full.wvrt_length); }
		if (strcmp(argv[1], "empty") == 0) { (void)wgf2udp8pjforward_write(empty, 0); }
		return 2;
	}
	uint8_t written = wgf2udp8pjforward_write(full, 1);
	printf("%d %u %u %u %u\n", wove_emitted_main(argc, argv), (unsigned)written,
		(unsigned)data[0], (unsigned)data[1], (unsigned)data[2]);
	return 0;
}

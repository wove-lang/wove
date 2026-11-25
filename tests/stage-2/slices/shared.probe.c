// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#undef main
int wove_emitted_main(int argc, char **argv);
uint8_t wgf1tb8104forward_read(wvrt_slice view, int32_t index);
int32_t wgf1tlecdcforward_length(wvrt_slice view);
_Bool wgf0oorrvmhas_last_byte(wvrt_slice view, uint8_t value);

// Supply native slice descriptors to the emitted functions and their reborrows.
int main(int argc, char **argv) {
	const uint8_t data[] = {7, 9, UINT8_MAX};
	const wvrt_slice full = {data, 3, sizeof *data};
	const wvrt_slice empty = {NULL, 0, sizeof *data};
	// Each requested out-of-bounds read must terminate in the Wove runtime.
	if (argc == 2) {
		if (strcmp(argv[1], "negative") == 0) { (void)wgf1tb8104forward_read(full, -1); }
		if (strcmp(argv[1], "end") == 0) { (void)wgf1tb8104forward_read(full, full.wvrt_length); }
		if (strcmp(argv[1], "empty") == 0) { (void)wgf1tb8104forward_read(empty, 0); }
		return 2;
	}
	printf("%d %u %" PRId32 " %" PRId32 " %d %d\n", wove_emitted_main(argc, argv),
		(unsigned)wgf1tb8104forward_read(full, 2),
		wgf1tlecdcforward_length(full), wgf1tlecdcforward_length(empty),
		wgf0oorrvmhas_last_byte(full, UINT8_MAX), wgf0oorrvmhas_last_byte(empty, UINT8_MAX));
	return 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
uint32_t wgf3nmgnnhread_kind(uint32_t);
int32_t wgf33d8lh5read_start(int32_t);
_Bool wgf0iks0atflags(_Bool, uint8_t);
int32_t wgf06llupvordered(wvrt_mut_slice);

// Observe native field widths and the effects of reordered constructor operands.
int main(int argc, char **argv) {
	uint8_t counter = 0;
	const wvrt_mut_slice state = {&counter, 1, sizeof counter};
	const int32_t ordered = wgf06llupvordered(state);
	return printf("%d %" PRIu32 " %" PRIu32 " %" PRId32 " %" PRId32 " %d %d %d %" PRId32 " %u\n",
		wove_emitted_main(argc, argv), wgf3nmgnnhread_kind(UINT32_MAX), wgf3nmgnnhread_kind(UINT32_C(2147483648)),
		wgf33d8lh5read_start(INT32_MIN), wgf33d8lh5read_start(INT32_MAX),
		wgf0iks0atflags(1, UINT8_MAX), wgf0iks0atflags(0, UINT8_MAX), wgf0iks0atflags(1, 0),
		ordered, (unsigned)counter) < 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
uint32_t wgf0_read_kind(uint32_t);
int32_t wgf1_read_start(int32_t);
_Bool wgf2_flags(_Bool, uint8_t);
int32_t wgf4_ordered(wvrt_mut_slice);

// Observe native field widths and the effects of reordered constructor operands.
int main(int argc, char **argv) {
	uint8_t counter = 0;
	const wvrt_mut_slice state = {&counter, 1, sizeof counter};
	const int32_t ordered = wgf4_ordered(state);
	return printf("%d %" PRIu32 " %" PRIu32 " %" PRId32 " %" PRId32 " %d %d %d %" PRId32 " %u\n",
		wove_emitted_main(argc, argv), wgf0_read_kind(UINT32_MAX), wgf0_read_kind(UINT32_C(2147483648)),
		wgf1_read_start(INT32_MIN), wgf1_read_start(INT32_MAX),
		wgf2_flags(1, UINT8_MAX), wgf2_flags(0, UINT8_MAX), wgf2_flags(1, 0),
		ordered, (unsigned)counter) < 0;
}

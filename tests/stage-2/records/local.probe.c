// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
uint32_t wgf0srj3igread_kind(uint32_t);
int32_t wgf1pllhgaread_start(int32_t);
_Bool wgf0dfnuekflags(_Bool, uint8_t);
int32_t wgf15thu6iordered(wvrt_mut_slice);

// Observe native field widths and the effects of reordered constructor operands.
int main(int argc, char **argv) {
	uint8_t counter = 0;
	const wvrt_mut_slice state = {&counter, 1, sizeof counter};
	const int32_t ordered = wgf15thu6iordered(state);
	return printf("%d %" PRIu32 " %" PRIu32 " %" PRId32 " %" PRId32 " %d %d %d %" PRId32 " %u\n",
		wove_emitted_main(argc, argv), wgf0srj3igread_kind(UINT32_MAX), wgf0srj3igread_kind(UINT32_C(2147483648)),
		wgf1pllhgaread_start(INT32_MIN), wgf1pllhgaread_start(INT32_MAX),
		wgf0dfnuekflags(1, UINT8_MAX), wgf0dfnuekflags(0, UINT8_MAX), wgf0dfnuekflags(1, 0),
		ordered, (unsigned)counter) < 0;
}

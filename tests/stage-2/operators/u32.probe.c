// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
uint32_t wgf00ff799add(uint32_t, uint32_t);
uint32_t wgf2pe5394subtract(uint32_t, uint32_t);
uint32_t wgf15d752rordered_add(wvrt_mut_slice);
uint32_t wgf23du0uiordered_subtract(wvrt_mut_slice);

// Report the full-width wrap results and count the effects of each operand.
int main(int argc, char **argv) {
	uint8_t add_state = 0;
	uint8_t sub_state = 0;
	const wvrt_mut_slice addition = {&add_state, 1, sizeof add_state};
	const wvrt_mut_slice subtraction = {&sub_state, 1, sizeof sub_state};
	const uint32_t ordered_add = wgf15d752rordered_add(addition);
	const uint32_t ordered_subtract = wgf23du0uiordered_subtract(subtraction);
	return printf("%d %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %" PRIu32 " %u %u\n",
		wove_emitted_main(argc, argv), wgf00ff799add(UINT32_MAX, 1), wgf2pe5394subtract(0, 1),
		wgf00ff799add(INT32_MAX, 1), wgf2pe5394subtract(UINT32_MAX, INT32_MAX),
		ordered_add, ordered_subtract, (unsigned)add_state, (unsigned)sub_state) < 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
int32_t wgf1_widen_i32(uint8_t);
uint32_t wgf2_widen_u32(uint8_t);
uint8_t wgf3_narrow_i32(int32_t);
uint8_t wgf4_narrow_u32(uint32_t);
int32_t wgf5_from_word(uint32_t);
uint32_t wgf6_from_signed(int32_t);
int32_t wgf7_same_signed(int32_t);
uint32_t wgf8_same_word(uint32_t);

// Expose the full-width results of dynamic conversions at their target limits.
int main(int argc, char **argv) {
	return printf("%d %" PRId32 " %" PRIu32 " %u %u %u %" PRId32 " %" PRIu32 " %" PRId32 " %" PRIu32 "\n",
		wove_emitted_main(argc, argv), wgf1_widen_i32(UINT8_MAX), wgf2_widen_u32(UINT8_MAX),
		(unsigned)wgf3_narrow_i32(UINT8_MAX), (unsigned)wgf4_narrow_u32(UINT8_MAX),
		(unsigned)wgf3_narrow_i32(0), wgf5_from_word(INT32_MAX),
		wgf6_from_signed(INT32_MAX), wgf7_same_signed(INT32_MIN), wgf8_same_word(UINT32_MAX)) < 0;
}

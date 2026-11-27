// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
int32_t wgf21pd0pfwiden_i32(uint8_t);
uint32_t wgf0e3987jwiden_u32(uint8_t);
uint8_t wgf35t2jilnarrow_i32(int32_t);
uint8_t wgf1jvderhnarrow_u32(uint32_t);
int32_t wgf0e2cerqfrom_word(uint32_t);
uint32_t wgf366gt7ifrom_signed(int32_t);
int32_t wgf3rk8t88same_signed(int32_t);
uint32_t wgf3spojd0same_word(uint32_t);

// Expose the full-width results of dynamic conversions at their target limits.
int main(int argc, char **argv) {
	return printf("%d %" PRId32 " %" PRIu32 " %u %u %u %" PRId32 " %" PRIu32 " %" PRId32 " %" PRIu32 "\n",
		wove_emitted_main(argc, argv), wgf21pd0pfwiden_i32(UINT8_MAX), wgf0e3987jwiden_u32(UINT8_MAX),
		(unsigned)wgf35t2jilnarrow_i32(UINT8_MAX), (unsigned)wgf1jvderhnarrow_u32(UINT8_MAX),
		(unsigned)wgf35t2jilnarrow_i32(0), wgf0e2cerqfrom_word(INT32_MAX),
		wgf366gt7ifrom_signed(INT32_MAX), wgf3rk8t88same_signed(INT32_MIN), wgf3spojd0same_word(UINT32_MAX)) < 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
int32_t wgf37vm820widen_i32(uint8_t);
uint32_t wgf0485pckwiden_u32(uint8_t);
uint8_t wgf2gnecdknarrow_i32(int32_t);
uint8_t wgf3eochj0narrow_u32(uint32_t);
int32_t wgf3ng6qi9from_word(uint32_t);
uint32_t wgf23e27lhfrom_signed(int32_t);
int32_t wgf2db2ltvsame_signed(int32_t);
uint32_t wgf3idbrqnsame_word(uint32_t);

// Expose the full-width results of dynamic conversions at their target limits.
int main(int argc, char **argv) {
	return printf("%d %" PRId32 " %" PRIu32 " %u %u %u %" PRId32 " %" PRIu32 " %" PRId32 " %" PRIu32 "\n",
		wove_emitted_main(argc, argv), wgf37vm820widen_i32(UINT8_MAX), wgf0485pckwiden_u32(UINT8_MAX),
		(unsigned)wgf2gnecdknarrow_i32(UINT8_MAX), (unsigned)wgf3eochj0narrow_u32(UINT8_MAX),
		(unsigned)wgf2gnecdknarrow_i32(0), wgf3ng6qi9from_word(INT32_MAX),
		wgf23e27lhfrom_signed(INT32_MAX), wgf2db2ltvsame_signed(INT32_MIN), wgf3idbrqnsame_word(UINT32_MAX)) < 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(void);
int32_t wgf17vm820widen_i32(uint8_t);
uint32_t wgf0485pckwiden_u32(uint8_t);
uint8_t wgf0gnecdknarrow_i32(int32_t);
uint8_t wgf1eochj0narrow_u32(uint32_t);
int32_t wgf1ng6qi9from_word(uint32_t);
uint32_t wgf03e27lhfrom_signed(int32_t);
int32_t wgf0db2ltvsame_signed(int32_t);
uint32_t wgf1idbrqnsame_word(uint32_t);

// Expose the full-width results of dynamic conversions at their target limits.
int main(void) {
	return printf("%d %" PRId32 " %" PRIu32 " %u %u %u %" PRId32 " %" PRIu32 " %" PRId32 " %" PRIu32 "\n",
		wove_emitted_main(), wgf17vm820widen_i32(UINT8_MAX), wgf0485pckwiden_u32(UINT8_MAX),
		(unsigned)wgf0gnecdknarrow_i32(UINT8_MAX), (unsigned)wgf1eochj0narrow_u32(UINT8_MAX),
		(unsigned)wgf0gnecdknarrow_i32(0), wgf1ng6qi9from_word(INT32_MAX),
		wgf03e27lhfrom_signed(INT32_MAX), wgf0db2ltvsame_signed(INT32_MIN), wgf1idbrqnsame_word(UINT32_MAX)) < 0;
}

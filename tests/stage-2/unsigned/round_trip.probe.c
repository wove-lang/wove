// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
uint8_t wgf1_byte_max(void);
uint32_t wgf2_word_high(void);
uint32_t wgf3_word_max(void);
uint8_t wgf4_exchange_byte(uint8_t);
uint32_t wgf5_exchange_word(uint32_t);

// Read the exact unsigned results through the native function ABI.
int main(int argc, char **argv) {
	return printf("%d %u %u %" PRIu32 " %" PRIu32 " %" PRIu32 "\n",
		wove_emitted_main(argc, argv),
		(unsigned)wgf4_exchange_byte(0), (unsigned)wgf1_byte_max(),
		wgf5_exchange_word(0), wgf2_word_high(), wgf3_word_max()) < 0;
}

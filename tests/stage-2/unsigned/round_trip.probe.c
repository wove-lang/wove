// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
uint8_t wgf2hus7msbyte_max(void);
uint32_t wgf39i6pnsword_high(void);
uint32_t wgf3h59872word_max(void);
uint8_t wgf3vvnadfexchange_byte(uint8_t);
uint32_t wgf2o1vkqhexchange_word(uint32_t);

// Read the exact unsigned results through the native function ABI.
int main(int argc, char **argv) {
	return printf("%d %u %u %" PRIu32 " %" PRIu32 " %" PRIu32 "\n",
		wove_emitted_main(argc, argv),
		(unsigned)wgf3vvnadfexchange_byte(0), (unsigned)wgf2hus7msbyte_max(),
		wgf2o1vkqhexchange_word(0), wgf39i6pnsword_high(), wgf3h59872word_max()) < 0;
}

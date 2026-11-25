// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
uint8_t wgf1ult6a7byte_max(void);
uint32_t wgf17j44u1word_high(void);
uint32_t wgf1ap3q8dword_max(void);
uint8_t wgf0jir086exchange_byte(uint8_t);
uint32_t wgf0r8sra8exchange_word(uint32_t);

// Read the exact unsigned results through the native function ABI.
int main(int argc, char **argv) {
	return printf("%d %u %u %" PRIu32 " %" PRIu32 " %" PRIu32 "\n",
		wove_emitted_main(argc, argv),
		(unsigned)wgf0jir086exchange_byte(0), (unsigned)wgf1ult6a7byte_max(),
		wgf0r8sra8exchange_word(0), wgf17j44u1word_high(), wgf1ap3q8dword_max()) < 0;
}

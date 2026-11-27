// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
_Bool wgf37losq1byte_lt(uint8_t, uint8_t);
_Bool wgf3bum8vbbyte_eq(uint8_t, uint8_t);
_Bool wgf2vrf5ljword_lt(uint32_t, uint32_t);
_Bool wgf1qs755dword_eq(uint32_t, uint32_t);

// Report unsigned ordering across the high bits and equality at the limits.
int main(int argc, char **argv) {
	const uint32_t high_bit = UINT32_C(2147483648);
	return printf("%d %d %d %d %d %d %d %d %d %d %d\n",
		wove_emitted_main(argc, argv),
		wgf37losq1byte_lt(127, 128), wgf37losq1byte_lt(128, 127), wgf37losq1byte_lt(128, 255),
		wgf3bum8vbbyte_eq(255, 255), wgf3bum8vbbyte_eq(255, 127),
		wgf2vrf5ljword_lt(UINT32_C(2147483647), high_bit), wgf2vrf5ljword_lt(high_bit, UINT32_C(2147483647)),
		wgf2vrf5ljword_lt(high_bit, UINT32_MAX),
		wgf1qs755dword_eq(UINT32_MAX, UINT32_MAX), wgf1qs755dword_eq(UINT32_MAX, high_bit)) < 0;
}

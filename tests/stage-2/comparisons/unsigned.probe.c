// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
_Bool wgf0s1bthmbyte_lt(uint8_t, uint8_t);
_Bool wgf0noehccbyte_eq(uint8_t, uint8_t);
_Bool wgf35cf9r0word_lt(uint32_t, uint32_t);
_Bool wgf12cuv5eword_eq(uint32_t, uint32_t);

// Report unsigned ordering across the high bits and equality at the limits.
int main(int argc, char **argv) {
	const uint32_t high_bit = UINT32_C(2147483648);
	return printf("%d %d %d %d %d %d %d %d %d %d %d\n",
		wove_emitted_main(argc, argv),
		wgf0s1bthmbyte_lt(127, 128), wgf0s1bthmbyte_lt(128, 127), wgf0s1bthmbyte_lt(128, 255),
		wgf0noehccbyte_eq(255, 255), wgf0noehccbyte_eq(255, 127),
		wgf35cf9r0word_lt(UINT32_C(2147483647), high_bit), wgf35cf9r0word_lt(high_bit, UINT32_C(2147483647)),
		wgf35cf9r0word_lt(high_bit, UINT32_MAX),
		wgf12cuv5eword_eq(UINT32_MAX, UINT32_MAX), wgf12cuv5eword_eq(UINT32_MAX, high_bit)) < 0;
}

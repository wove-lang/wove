// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>

#define main wove_fixture_main
#include "scalar.convert.c"
#undef main

// Match the seed's generated names to the fixture declarations.
#define u8_to_i32 wv_seed_fn_0
#define u8_to_u32 wv_seed_fn_1
#define i32_to_u8 wv_seed_fn_2
#define u32_to_u8 wv_seed_fn_3
#define u32_to_i32 wv_seed_fn_4
#define i32_to_u32 wv_seed_fn_5

// Name the conversion that changed a representable value.
static _Noreturn void fail(const char *conversion) {
	fprintf(stderr, "unexpected value from %s\n", conversion);
	exit(EXIT_FAILURE);
}

// Check widening and the dynamic endpoints of the narrowing paths.
int main(void) {
	if (wove_fixture_main() != UINT8_MAX) { fail("u8 to i32"); }
	if (u8_to_u32(UINT8_MAX) != UINT32_C(255)) { fail("u8 to u32"); }
	if (i32_to_u8(0) != 0 || i32_to_u8(255) != UINT8_MAX) { fail("i32 to u8"); }
	if (u32_to_u8(0) != 0 || u32_to_u8(255) != UINT8_MAX) { fail("u32 to u8"); }
	if (u32_to_i32(0) != 0 || u32_to_i32(UINT32_C(2147483647)) != INT32_MAX) { fail("u32 to i32"); }
	if (i32_to_u32(0) != 0 || i32_to_u32(INT32_MAX) != UINT32_C(2147483647)) { fail("i32 to u32"); }
	return 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>

#define main wove_fixture_main
#include "scalar.round_trip.c"
#undef main

// Stage 1 emits the fixture's functions in declaration order.
#define echo_u8 wv_seed_fn_0
#define u8_min wv_seed_fn_1
#define u8_max wv_seed_fn_2
#define echo_i32 wv_seed_fn_3
#define i32_min wv_seed_fn_4
#define i32_max wv_seed_fn_5
#define echo_u32 wv_seed_fn_6
#define u32_min wv_seed_fn_7
#define u32_max wv_seed_fn_8

_Static_assert(_Generic(&echo_u8, uint8_t (*)(uint8_t): 1, default: 0), "u8 call signature");
_Static_assert(_Generic(&echo_i32, int32_t (*)(int32_t): 1, default: 0), "i32 call signature");
_Static_assert(_Generic(&echo_u32, uint32_t (*)(uint32_t): 1, default: 0), "u32 call signature");
_Static_assert(_Generic(&u32_max, uint32_t (*)(void): 1, default: 0), "u32 result signature");

// Report which scalar type failed to preserve a value.
static _Noreturn void fail(const char *kind) {
	fprintf(stderr, "unexpected %s round-trip value\n", kind);
	exit(EXIT_FAILURE);
}

// Inspect the full return values after Wove passes the boundary literals through calls.
int main(int argc, char **argv) {
	if (u8_min() != 0 || u8_max() != UINT8_MAX) { fail("u8"); }
	if (i32_min() != INT32_MIN || i32_max() != INT32_MAX) { fail("i32"); }
	if (u32_min() != 0 || u32_max() != UINT32_MAX) { fail("u32"); }
	if (echo_u32(UINT32_C(2147483649)) != UINT32_C(2147483649)) { fail("u32 high bit"); }
	return wove_fixture_main(argc, argv);
}

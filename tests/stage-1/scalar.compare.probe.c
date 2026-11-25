// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>

#define main wove_fixture_main
#include "scalar.compare.c"
#undef main

// Match the seed's generated names to the fixture declarations.
#define echo_bool wv_seed_fn_0
#define literal_true wv_seed_fn_1
#define literal_false wv_seed_fn_2
#define u8_eq wv_seed_fn_3
#define u8_lt wv_seed_fn_4
#define i32_eq wv_seed_fn_5
#define i32_lt wv_seed_fn_6
#define u32_eq wv_seed_fn_7
#define u32_lt wv_seed_fn_8

_Static_assert(_Generic(&echo_bool, bool (*)(bool): 1, default: 0), "bool call signature");
_Static_assert(_Generic(&u32_lt, bool (*)(uint32_t, uint32_t): 1, default: 0), "bool comparison result");

// Identify the boolean or comparison that failed its expected result.
static _Noreturn void fail(const char *operation) {
	fprintf(stderr, "unexpected result from %s\n", operation);
	exit(EXIT_FAILURE);
}

// Observe both boolean values through Wove calls and check numeric ordering.
int main(int argc, char **argv) {
	if (!literal_true() || literal_false()) { fail("boolean literals"); }
	if (!u8_eq(40, 40) || u8_eq(40, 41)) { fail("u8.eq"); }
	if (!u8_lt(127, 128) || !u8_lt(128, 255) || u8_lt(255, 128)) { fail("u8.lt"); }
	if (!i32_eq(-1, -1) || i32_eq(-1, 0)) { fail("i32.eq"); }
	if (!i32_lt(-1, 0) || i32_lt(0, -1)) { fail("i32.lt"); }

	uint32_t high = UINT32_C(2147483648);
	if (!u32_eq(high, high) || u32_eq(high, UINT32_MAX)) { fail("u32.eq"); }
	// Check ordering across the high bit as well as between two high-bit values.
	if (!u32_lt(0, high) || !u32_lt(high, UINT32_MAX) || u32_lt(high, 0)) {
		fail("u32.lt");
	}
	return wove_fixture_main(argc, argv);
}

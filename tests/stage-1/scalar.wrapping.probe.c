// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>

#define main wove_fixture_main
#include "scalar.wrapping.c"
#undef main

// Match the seed's generated names to the fixture declarations.
#define advance wv_seed_fn_0
#define distance wv_seed_fn_1
#define add_unsigned wv_seed_fn_2
#define sub_unsigned wv_seed_fn_3

// Identify the operation that produced an unexpected value.
static _Noreturn void fail(const char *operation) {
	fprintf(stderr, "unexpected result from %s\n", operation);
	exit(EXIT_FAILURE);
}

// Check full-width results when both operands enter through parameters.
int main(int argc, char **argv) {
	if (wove_fixture_main(argc, argv) != 18) { fail("signed cursor distance"); }
	if (advance(INT32_MAX, 1) != INT32_MIN) { fail("i32 maximum plus one"); }
	if (advance(INT32_MIN, -1) != INT32_MAX) { fail("i32 minimum plus negative one"); }
	if (distance(INT32_MIN, 1) != INT32_MAX) { fail("i32 minimum minus one"); }
	if (distance(INT32_MAX, -1) != INT32_MIN) { fail("i32 maximum minus negative one"); }
	if (add_unsigned(UINT32_MAX, 1) != 0) { fail("u32 maximum plus one"); }
	if (sub_unsigned(0, 1) != UINT32_MAX) { fail("u32 zero minus one"); }
	return 0;
}

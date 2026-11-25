// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <string.h>

#define main wove_fixture_main
#include "module.bindings.c"
#undef main

#define alphabet wv_seed_binding_0
#define alphabet_bytes wv_seed_binding_0_bytes
#define digit wv_seed_fn_0
#define signed_byte wv_seed_fn_3

_Static_assert(sizeof alphabet_bytes == 16, "alphabet backing length");
_Static_assert(_Generic(&alphabet_bytes[0], const uint8_t *: 1, default: 0), "read-only byte storage");
_Static_assert(_Generic(&alphabet, const wvrt_slice *: 1, default: 0), "read-only view");
_Static_assert(_Generic(&signed_byte, int8_t (*)(int8_t): 1, default: 0), "signed byte signature");

// Check the generated view and its indexed reads against the exact alphabet.
int main(int argc, char **argv) {
	const char expected[] = "0123456789abcdef";
	if (alphabet.wvrt_items != alphabet_bytes) { return 18; }
	if (memcmp(alphabet_bytes, expected, sizeof alphabet_bytes) != 0) { return 19; }
	// Read every nibble through the usual Wove slice operation.
	for (int32_t index = 0; index < alphabet.wvrt_length; ++index) {
		if (digit(index) != (uint8_t)expected[index]) { return 20; }
	}
	return wove_fixture_main(argc, argv);
}

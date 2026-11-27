// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

typedef signed _BitInt(256) FixtureSigned;
typedef unsigned _BitInt(256) FixtureUnsigned;

_Static_assert(sizeof(FixtureSigned) == 32 && sizeof(FixtureUnsigned) == 32, "exact width");

// Inspect the ABI values delivered by the generated Wove caller.
static inline int check_wide(FixtureSigned minimum, FixtureSigned maximum,
		FixtureUnsigned largest, FixtureUnsigned binary) {
	const FixtureSigned signed_maximum = (FixtureSigned)(((FixtureUnsigned)1 << 255) - 1);
	const FixtureUnsigned bits = ((FixtureUnsigned)1 << 255) |
		((FixtureUnsigned)1 << 192) | ((FixtureUnsigned)1 << 160) |
		((FixtureUnsigned)1 << 128) | ((FixtureUnsigned)1 << 96) |
		((FixtureUnsigned)1 << 64) | ((FixtureUnsigned)1 << 32) | 1;
	return minimum == -signed_maximum - 1 && maximum == signed_maximum &&
		largest == ~(FixtureUnsigned)0 && binary == bits ? 42 : 1;
}

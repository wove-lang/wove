// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t FixtureCount;

// Compare the entire signed native value without narrowing it.
static inline bool equal_signed(signed _BitInt(256) actual, signed _BitInt(256) expected) {
	return actual == expected;
}

// Compare the entire unsigned native value without narrowing it.
static inline bool equal_unsigned(unsigned _BitInt(256) actual, unsigned _BitInt(256) expected) {
	return actual == expected;
}

// Check every byte against an asymmetric sequence that starts with the sign bit set.
static inline bool check_pattern(const uint8_t *bytes, int32_t count, bool big_endian) {
	if (count != 32) { return false; }
	// Each endian encoding has a different byte at every index.
	for (int32_t index = 0; index < count; ++index) {
		const int32_t offset = big_endian ? index : count - 1 - index;
		if (bytes[index] != (uint8_t)(0x80 + offset)) { return false; }
	}
	return true;
}

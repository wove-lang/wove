// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Prevent a constant-folded signed operand from hiding backend requirements.
static inline signed _BitInt(256) varying_signed(signed _BitInt(256) word) {
	volatile signed _BitInt(256) value = word;
	return value;
}

// Keep the unsigned operand just as dynamic as the signed operand.
static inline unsigned _BitInt(256) varying_unsigned(unsigned _BitInt(256) word) {
	volatile unsigned _BitInt(256) value = word;
	return value;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TERMINATED_ARRAYS_H
#define WOVE_TEST_TERMINATED_ARRAYS_H

#include <errno.h>
#include <stdint.h>

typedef int16_t FixtureUnit;
typedef uint32_t FixtureWord;

// Check both terminated inputs and return different offsets into their respective copies.
static inline int32_t fixture_scan(FixtureUnit **prefix, const FixtureUnit *text,
	const FixtureWord *word, FixtureWord **suffix) {
	if (text[0] != 256 || text[1] != 17 || text[2] != -512 || text[3] != 0) { return 1; }
	if (word[0] != UINT32_C(65536) || word[1] != 0) { return 2; }
	if (prefix != 0) { *prefix = (FixtureUnit *)text + 2; }
	if (suffix != 0) { *suffix = (FixtureWord *)word + 1; }
	errno = EDOM;
	return 42;
}

// An empty view still supplies a valid pointer to one zero element.
static inline int32_t fixture_empty(const FixtureUnit *text, FixtureUnit **end) {
	if (end != 0) { *end = (FixtureUnit *)text; }
	return text[0] == 0 ? 42 : 3;
}

// A terminated array does not require a native end-pointer formal.
static inline int32_t fixture_no_end(const FixtureWord *word) {
	return word[0] == UINT32_C(65536) && word[1] == 0 ? 42 : 4;
}

// Exercise a pointer beyond the terminator, before the input, foreign, misaligned, or unwritten.
static inline int32_t fixture_bad(const FixtureUnit *text, FixtureUnit **end, int32_t mode) {
	static FixtureUnit other;
	switch (mode) {
		case 1: *end = (FixtureUnit *)text + 3; break;
		case 2: *end = (FixtureUnit *)((uintptr_t)text - sizeof(*text)); break;
		case 3: *end = &other; break;
		case 4: *end = (FixtureUnit *)((uintptr_t)text + 1); break;
		default: break;
	}
	return 42;
}

// A pointer-to-const output must not match the mutable native end slot.
static inline int32_t fixture_wrong(const FixtureUnit *text, const FixtureUnit **end) {
	*end = text;
	return 42;
}

#endif

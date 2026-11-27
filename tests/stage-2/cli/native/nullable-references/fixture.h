// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_FIXTURE_NULLABLE_REFERENCES_H
#define WOVE_FIXTURE_NULLABLE_REFERENCES_H

#include <stdint.h>
#include <stddef.h>

typedef uint64_t fixture_key;
struct fixture_pair {
	uint32_t tag;
};

#ifdef WOVE_FIXTURE_INCOMPLETE
struct fixture_empty;
#else
struct fixture_empty { uint32_t value; };
#endif

// The bridge can borrow a complete native type without projecting its members.
static inline int32_t fixture_empty_probe(const struct fixture_empty *record)
{
	return record == NULL ? 42 : 1;
}

static uintptr_t expected_key;
static uintptr_t expected_input;
static uintptr_t expected_output;

// Remember each caller place while all three can be shared.
static inline void fixture_remember(const fixture_key *key, const struct fixture_pair *input,
	const struct fixture_pair *output)
{
	expected_key = (uintptr_t)key;
	expected_input = (uintptr_t)input;
	expected_output = (uintptr_t)output;
}

// Observe independently nullable operands and write through the original mutable pointer.
static inline int32_t fixture_probe(const fixture_key *key, const struct fixture_pair *input,
#ifdef WOVE_FIXTURE_CONST_OUTPUT
	const struct fixture_pair *output)
#else
	struct fixture_pair *output)
#endif
{
	// Read only live operands and compare their addresses with the original places.
	if ((key != NULL && ((uintptr_t)key != expected_key || *key != UINT64_C(4294967313)))
		|| (input != NULL && ((uintptr_t)input != expected_input || input->tag != 31))
		|| (output != NULL && (uintptr_t)output != expected_output)) {
		return -1;
	}
#ifndef WOVE_FIXTURE_CONST_OUTPUT
	// Only a present mutable loan allows the native write.
	if (output != NULL) {
		output->tag = 77;
	}
#endif
	return (key != NULL) + 2 * (input != NULL) + 4 * (output != NULL);
}

#endif

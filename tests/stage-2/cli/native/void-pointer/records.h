// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_VOID_POINTER_RECORDS_H
#define WOVE_TEST_VOID_POINTER_RECORDS_H

#include <stdbool.h>
#include <stdint.h>

struct record_sample {
	int32_t tag;
	uint64_t value;
};

struct other_sample {
	int32_t tag;
	uint64_t value;
};

// Verify that both loans refer to the same aligned object.
static inline bool same_record(const void *raw, const struct record_sample *typed)
{
	const struct record_sample *sample = raw;
	return sample == typed && (uintptr_t)raw % _Alignof(struct record_sample) == 0
		&& sample->tag == 17 && sample->value == UINT64_C(4294967307);
}

// Use the actual C layout for both reads and writes.
static inline void copy_record(const void *raw_input, void *raw_output)
{
	const struct record_sample *input = raw_input;
	struct record_sample *output = raw_output;
	// The caller receives the update only when the original output storage was lent.
	if ((uintptr_t)raw_output % _Alignof(struct record_sample) == 0) {
		output->tag = input->tag + 25;
		output->value = input->value + 1;
	}
}

// A mutable native formal is incompatible with a shared Wove loan.
static inline bool wrong_shared_record(void *raw)
{
	return raw != 0;
}

// A const native formal is incompatible with a mutable Wove loan.
static inline bool wrong_mutable_record(const void *raw)
{
	return raw != 0;
}

#endif

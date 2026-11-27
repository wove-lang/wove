// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>
#include <stdlib.h>

static int32_t inout_entries;

// Report each native call so the successful sequence has an exact count.
static inline int32_t observed_entries(void) {
	return inout_entries;
}

// Write three elements on success or return their required count for a retry.
static inline int32_t inout_groups(uint16_t *groups, int32_t *count) {
	++inout_entries;
	// A contract violation must trap in Wove before this sentinel can run.
	if (*count < 0 || *count > 3) {
		_Exit(91);
	}
	// An undersized array is untouched, but the count must be updated.
	if (*count < 3) {
		*count = 3;
		return -31;
	}
	groups[0] = 17;
	groups[1] = 513;
	groups[2] = 65535;
	*count = 3;
	return 29;
}

// Accept the full-width native count ABI and update the caller's slot.
static inline int32_t inout_wide(uint16_t *groups, uint64_t *count) {
	++inout_entries;
	// Use a separate sentinel to detect an unchecked wide pointee value.
	if (*count != 1) {
		_Exit(92);
	}
	groups[0] = 42;
	*count = 0;
	return 17;
}

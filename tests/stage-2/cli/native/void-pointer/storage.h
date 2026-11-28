// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_RECORD_STORAGE_H
#define WOVE_TEST_RECORD_STORAGE_H

#include <stdint.h>

typedef void *storage_carrier;

struct native_storage {
	uint64_t first;
	uint64_t last;
};

// Observe the original aligned address before the exclusive call.
static inline uint64_t storage_address(const struct native_storage *record)
{
	return (uintptr_t)record;
}

// Provide an old-style declaration for the compatibility regression.
#if defined(WOVE_TEST_STORAGE_UNPROTOTYPED)
int32_t storage_write();
#else
// Write both ends of the caller's complete record and return the original status.
static inline int32_t storage_write(int32_t status, storage_carrier *raw, uint64_t address)
{
	// Both the actual address and its native alignment must survive the bridge.
	if ((uintptr_t)raw != address || address % _Alignof(struct native_storage) != 0) {
		return 99;
	}
	struct native_storage *record = (void *)raw;
	record->first += UINT64_C(4294967296);
	record->last = UINT64_C(0xFEDCBA9876543210);
	return status;
}
#endif

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_RECORD_INPUT_FIXTURE_H
#define WOVE_RECORD_INPUT_FIXTURE_H

#include <stddef.h>
#include <stdint.h>

struct wove_byte_request {
	int32_t marker;
	void *bytes;
	size_t size;
};

struct wove_range {
	int32_t offset;
	uint64_t length;
};

struct wove_range_request {
	int32_t marker;
	const struct wove_range *ranges;
	uint32_t count;
};

static uintptr_t original_record;
static uintptr_t original_data;

static inline void remember_bytes_request(struct wove_byte_request *record) {
	original_record = (uintptr_t)record;
}

static inline void remember_range_request(struct wove_range_request *record) {
	original_record = (uintptr_t)record;
}

static inline void remember_bytes(const unsigned char *bytes, size_t size) {
	(void)size;
	original_data = (uintptr_t)bytes;
}

static inline void remember_ranges(const struct wove_range *ranges, uint32_t count) {
	(void)count;
	original_data = (uintptr_t)ranges;
}

static inline int32_t fill_request(struct wove_byte_request *record) {
	if ((uintptr_t)record != original_record || record->marker != 11 ||
		record->size != 2 || (uintptr_t)record->bytes != original_data)
		return -1;
	unsigned char *bytes = record->bytes;
	bytes[0] = 13;
	bytes[1] = 29;
	record->marker = 17;
	return -7;
}

static inline int32_t bytes_restored(const struct wove_byte_request *record) {
	return record->marker == 17 && record->bytes == NULL && record->size == 95;
}

static inline int32_t check_ranges(struct wove_range_request *record) {
	if ((uintptr_t)record != original_record || record->marker != 61 ||
		(uintptr_t)record->ranges != original_data)
		return -1;
	if (record->count == 0)
		return 24;
	if (record->count != 2 || record->ranges[0].offset != 14 ||
		record->ranges[0].length != UINT64_C(4294967297) ||
		record->ranges[1].offset != 23 || record->ranges[1].length != 59)
		return -2;
	return 42;
}

static inline int32_t ranges_restored(const struct wove_range_request *record) {
	return record->ranges == NULL && record->count == 51;
}

static inline int32_t alias_request(struct wove_byte_request *record, unsigned char *other, size_t size) {
	(void)other;
	(void)size;
	return fill_request(record);
}

#endif

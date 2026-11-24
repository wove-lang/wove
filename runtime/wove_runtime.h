// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WVRT_RUNTIME_H
#define WVRT_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	const char *wvrt_file;
	int wvrt_line;
	int wvrt_column;
} wvrt_location;

typedef struct WvrtArray {
	void *wvrt_items;
	int32_t wvrt_length;
	size_t wvrt_element_size;
	void (*wvrt_drop_element)(void *);
	// Borrowed bridge stubs initialize only the leading items, length, and size.
	int32_t wvrt_capacity;
	// Zero means no pending spare window; other values encode its length plus one.
	uint32_t wvrt_spare;
	size_t wvrt_element_alignment;
} wvrt_array;
typedef struct { const void *wvrt_items; int32_t wvrt_length; size_t wvrt_element_size; } wvrt_slice;
typedef struct { void *wvrt_items; int32_t wvrt_length; size_t wvrt_element_size; } wvrt_mut_slice;

wvrt_array *wvrt_array_new(int32_t wvrt_length, size_t wvrt_element_size,
	size_t wvrt_element_alignment, wvrt_location wvrt_site);
void wvrt_array_drop(wvrt_array *wvrt_buffer);
wvrt_slice wvrt_slice_range_u8(const void *wvrt_items, int32_t wvrt_length,
	int32_t wvrt_start, int32_t wvrt_end, wvrt_location wvrt_site);
size_t wvrt_slice_offset(int32_t wvrt_length, int32_t wvrt_index, wvrt_location wvrt_site);
uint8_t wvrt_slice_load_u8(const void *wvrt_items, int32_t wvrt_length, int32_t wvrt_index,
	wvrt_location wvrt_site);
void wvrt_slice_store_u8(void *wvrt_items, int32_t wvrt_length, int32_t wvrt_index,
	uint8_t wvrt_value, wvrt_location wvrt_site);
int64_t wvrt_int_convert(int64_t wvrt_value, int64_t wvrt_min, int64_t wvrt_max,
	wvrt_location wvrt_site);
int32_t wvrt_i32_add_wrapping(int32_t wvrt_left, int32_t wvrt_right);
int32_t wvrt_i32_sub_wrapping(int32_t wvrt_left, int32_t wvrt_right);
uint32_t wvrt_u32_add_wrapping(uint32_t wvrt_left, uint32_t wvrt_right);
uint32_t wvrt_u32_sub_wrapping(uint32_t wvrt_left, uint32_t wvrt_right);

#endif

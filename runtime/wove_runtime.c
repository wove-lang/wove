// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Report a runtime failure at the originating Wove location and exit.
static _Noreturn void wvrt_trap(wvrt_location wvrtsite, const char *wvrtmessage) {
	fprintf(stderr, "%s:%d:%d: runtime error: %s\n", wvrtsite.wvrt_file, wvrtsite.wvrt_line, wvrtsite.wvrt_column, wvrtmessage);
	exit(EXIT_FAILURE);
}

// Preserve C object alignment, including native types with extended alignment.
// The caller owns failure handling, so a newly acquired foreign resource can be closed.
static void *wvrt_calloc(size_t wvrtcount, size_t wvrtsize, size_t wvrtalignment) {
	if (wvrtcount > SIZE_MAX / wvrtsize) { errno = ENOMEM; return NULL; }
	if (wvrtalignment <= _Alignof(max_align_t)) { return calloc(wvrtcount, wvrtsize); }
	size_t wvrt_bytes = wvrtcount * wvrtsize;
	size_t wvrt_padding = (wvrtalignment - wvrt_bytes % wvrtalignment) % wvrtalignment;
	if (wvrt_bytes > SIZE_MAX - wvrt_padding) { errno = ENOMEM; return NULL; }
	wvrt_bytes += wvrt_padding;
	void *wvrt_storage = aligned_alloc(wvrtalignment, wvrt_bytes);
	if (wvrt_storage != NULL) { memset(wvrt_storage, 0, wvrt_bytes); }
	return wvrt_storage;
}

// Return failure to callers that must release foreign storage before reporting an allocation trap.
static wvrt_array *wvrt_array_try_new(int32_t wvrtlength, size_t wvrtelement_size,
	size_t wvrtelement_alignment) {
	if (wvrtlength < 0) { return NULL; }
	/* Keep an aligned, non-null address even for empty arrays and slices. */
	size_t wvrt_count = wvrtlength == 0 ? 1 : (size_t)wvrtlength;
	if (wvrt_count > SIZE_MAX / wvrtelement_size) { return NULL; }
	wvrt_array *wvrt_buffer = malloc(sizeof *wvrt_buffer);
	if (wvrt_buffer == NULL) { return NULL; }
	wvrt_buffer->wvrt_items = wvrt_calloc(wvrt_count, wvrtelement_size, wvrtelement_alignment);
	// The descriptor owns no backing storage yet, so release it before returning failure.
	if (wvrt_buffer->wvrt_items == NULL) {
		free(wvrt_buffer);
		return NULL;
	}
	wvrt_buffer->wvrt_length = wvrtlength;
	wvrt_buffer->wvrt_capacity = (int32_t)wvrt_count;
	wvrt_buffer->wvrt_spare = 0;
	wvrt_buffer->wvrt_element_size = wvrtelement_size;
	wvrt_buffer->wvrt_element_alignment = wvrtelement_alignment;
	wvrt_buffer->wvrt_drop_element = NULL;
	return wvrt_buffer;
}

// Allocate zero-initialized elements using their actual C size and alignment.
wvrt_array *wvrt_array_new(int32_t wvrtlength, size_t wvrtelement_size,
	size_t wvrtelement_alignment, wvrt_location wvrtsite) {
	if (wvrtlength < 0) { wvrt_trap(wvrtsite, "array length cannot be negative"); }
	size_t wvrt_count = wvrtlength == 0 ? 1 : (size_t)wvrtlength;
	if (wvrt_count > SIZE_MAX / wvrtelement_size) { wvrt_trap(wvrtsite, "array allocation size overflow"); }
	wvrt_array *wvrt_result = wvrt_array_try_new(wvrtlength, wvrtelement_size, wvrtelement_alignment);
	if (wvrt_result == NULL) { wvrt_trap(wvrtsite, "could not allocate array"); }
	return wvrt_result;
}

// Retire exactly the active owners in an element range.
static void wvrt_array_drop_range(wvrt_array *wvrtbuffer, int32_t wvrtstart, int32_t wvrtend) {
	if (wvrtbuffer->wvrt_drop_element == NULL) { return; }
	// Use the array's element stride and destructor for nested or native owners.
	for (int32_t wvrt_i = wvrtstart; wvrt_i < wvrtend; ++wvrt_i) {
		wvrtbuffer->wvrt_drop_element((unsigned char *)wvrtbuffer->wvrt_items +
			(size_t)wvrt_i * wvrtbuffer->wvrt_element_size);
	}
}

// Release each owned element before its containing storage and descriptor.
void wvrt_array_drop(wvrt_array *wvrtbuffer) {
	if (wvrtbuffer == NULL) { return; }
	wvrt_array_drop_range(wvrtbuffer, 0, wvrtbuffer->wvrt_length);
	free(wvrtbuffer->wvrt_items);
	free(wvrtbuffer);
}

// Form a shared view relative to the first byte of the checked range.
wvrt_slice wvrt_slice_range_u8(const void *wvrtitems, int32_t wvrtlength,
	int32_t wvrtstart, int32_t wvrtend, wvrt_location wvrtsite) {
	// Check both endpoints before subtracting or advancing the pointer.
	if (wvrtstart < 0 || wvrtend < wvrtstart || wvrtend > wvrtlength) {
		wvrt_trap(wvrtsite, "slice range out of bounds");
	}
	const uint8_t *wvrt_base = wvrtitems;
	if (wvrtstart != 0) { wvrt_base += (size_t)wvrtstart; }
	return (wvrt_slice){wvrt_base, wvrtend - wvrtstart, sizeof(uint8_t)};
}

// Validate the signed index before converting it to a byte offset.
static size_t wvrt_slice_offset(int32_t wvrtlength, int32_t wvrtindex, wvrt_location wvrtsite) {
	// Negative indices and the one-past endpoint cannot name a byte.
	if (wvrtindex < 0 || wvrtindex >= wvrtlength) {
		wvrt_trap(wvrtsite, "slice index out of bounds");
	}
	return (size_t)wvrtindex;
}

// Read one byte through a shared or mutable view of the original storage.
uint8_t wvrt_slice_load_u8(const void *wvrtitems, int32_t wvrtlength, int32_t wvrtindex,
	wvrt_location wvrtsite) {
	size_t wvrt_offset = wvrt_slice_offset(wvrtlength, wvrtindex, wvrtsite);
	return ((const uint8_t *)wvrtitems)[wvrt_offset];
}

// Update the original storage after checking the mutable view's byte index.
void wvrt_slice_store_u8(void *wvrtitems, int32_t wvrtlength, int32_t wvrtindex,
	uint8_t wvrtvalue, wvrt_location wvrtsite) {
	size_t wvrt_offset = wvrt_slice_offset(wvrtlength, wvrtindex, wvrtsite);
	((uint8_t *)wvrtitems)[wvrt_offset] = wvrtvalue;
}

// Check an integer in the common signed domain before the caller casts it.
int64_t wvrt_int_convert(int64_t wvrtvalue, int64_t wvrtmin, int64_t wvrtmax,
	wvrt_location wvrtsite) {
	// All seed integers fit in int64_t, so this comparison preserves their signs and high bits.
	if (wvrtvalue < wvrtmin || wvrtvalue > wvrtmax) {
		wvrt_trap(wvrtsite, "integer conversion out of range");
	}
	return wvrtvalue;
}

// Reduce the unsigned sum to its low 32 bits.
uint32_t wvrt_u32_add_wrapping(uint32_t wvrtleft, uint32_t wvrtright) {
	return (uint32_t)((uint64_t)wvrtleft + wvrtright);
}

// Reduce the unsigned difference to its low 32 bits.
uint32_t wvrt_u32_sub_wrapping(uint32_t wvrtleft, uint32_t wvrtright) {
	return (uint32_t)((uint64_t)wvrtleft - wvrtright);
}

// Interpret wrapped bits as a signed value using only in-range signed casts.
static int32_t wvrt_i32_from_wrapped_bits(uint32_t wvrtbits) {
	if (wvrtbits <= INT32_MAX) { return (int32_t)wvrtbits; }
	return -1 - (int32_t)(UINT32_MAX - wvrtbits);
}

// Add in the modulo domain and return the corresponding signed value.
int32_t wvrt_i32_add_wrapping(int32_t wvrtleft, int32_t wvrtright) {
	return wvrt_i32_from_wrapped_bits(wvrt_u32_add_wrapping((uint32_t)wvrtleft, (uint32_t)wvrtright));
}

// Subtract in the modulo domain and return the corresponding signed value.
int32_t wvrt_i32_sub_wrapping(int32_t wvrtleft, int32_t wvrtright) {
	return wvrt_i32_from_wrapped_bits(wvrt_u32_sub_wrapping((uint32_t)wvrtleft, (uint32_t)wvrtright));
}

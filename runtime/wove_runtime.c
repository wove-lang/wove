// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Report a runtime failure at the originating Wove location and exit.
static _Noreturn void wvrt_trap(wvrt_location wvrtsite, const char *wvrtmessage) {
	fprintf(stderr, "%s:%d:%d: runtime error: %s\n", wvrtsite.wvrt_file, wvrtsite.wvrt_line, wvrtsite.wvrt_column, wvrtmessage);
	exit(EXIT_FAILURE);
}

// Report a runtime failure at the original byte span of a Stage 2 operation.
static _Noreturn void wvrt_trap_at_span(int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend,
	const char *wvrtmessage) {
	fprintf(stderr, "file %" PRId32 " bytes [%" PRId32 ",%" PRId32 "): runtime error: %s\n",
		wvrtfile, wvrtstart, wvrtend, wvrtmessage);
	exit(EXIT_FAILURE);
}

// A borrowed native handle must be present before it is passed to C.
void wvrt_check_c_handle(const volatile void *wvrthandle, int32_t wvrtfile,
	int32_t wvrtstart, int32_t wvrtend) {
	if (wvrthandle == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "missing C handle"); }
}

// Validate the requested prefix before a C call borrows the original byte storage.
void wvrt_check_c_bytes(wvrt_slice wvrtview, int64_t wvrtsize, int64_t wvrtcount,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Each dimension is nonnegative even when the other dimension is zero.
	if (wvrtsize < 0 || wvrtcount < 0) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "negative C byte extent");
	}
	// A nonempty byte slice has a valid byte stride and a backing address.
	if (wvrtview.wvrt_length < 0 || (wvrtview.wvrt_length > 0 &&
		(wvrtview.wvrt_items == NULL || wvrtview.wvrt_element_size != sizeof(uint8_t)))) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "invalid C byte slice");
	}
	// Compare by division so even a product above INT64_MAX cannot overflow.
	if (wvrtsize != 0 && wvrtcount > (int64_t)wvrtview.wvrt_length / wvrtsize) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "C byte extent out of bounds");
	}
}

// Check every argument before allocating one short-lived backing store for the call.
char *wvrt_cstr_prepare(wvrt_cstr_argument *wvrtarguments, size_t wvrtcount) {
	if (wvrtcount == 0) { return NULL; }
	size_t wvrt_total = 0;
	// Validate byte views and the shared allocation extent before touching any bytes.
	for (size_t wvrt_i = 0; wvrt_i < wvrtcount; ++wvrt_i) {
		const wvrt_cstr_argument *wvrt_argument = &wvrtarguments[wvrt_i];
		const wvrt_slice wvrt_view = wvrt_argument->wvrt_view;
		// Empty views can use a null address. Nonempty views require byte storage.
		if (wvrt_view.wvrt_length < 0 || (wvrt_view.wvrt_length > 0 &&
			(wvrt_view.wvrt_items == NULL || wvrt_view.wvrt_element_size != sizeof(uint8_t)))) {
			wvrt_trap_at_span(wvrt_argument->wvrt_file, wvrt_argument->wvrt_start, wvrt_argument->wvrt_end,
				"invalid C string byte slice");
		}
		// Allow one additional byte for each argument's final NUL.
		if ((uintmax_t)wvrt_view.wvrt_length >= SIZE_MAX - wvrt_total) {
			wvrt_trap_at_span(wvrt_argument->wvrt_file, wvrt_argument->wvrt_start, wvrt_argument->wvrt_end,
				"C string allocation size overflow");
		}
		const size_t wvrt_length = (size_t)wvrt_view.wvrt_length;
		// Checked conversion rejects embedded NUL before entering native code.
		if (wvrt_argument->wvrt_checked && wvrt_length != 0 && memchr(wvrt_view.wvrt_items, 0, wvrt_length) != NULL) {
			wvrt_trap_at_span(wvrt_argument->wvrt_file, wvrt_argument->wvrt_start, wvrt_argument->wvrt_end,
				"embedded NUL in C string argument");
		}
		wvrt_total += wvrt_length + 1;
	}
	char *wvrt_storage = malloc(wvrt_total);
	// The first argument owns the allocation diagnostic for this call's scratch.
	if (wvrt_storage == NULL) {
		wvrt_trap_at_span(wvrtarguments[0].wvrt_file, wvrtarguments[0].wvrt_start, wvrtarguments[0].wvrt_end,
			"could not allocate C string arguments");
	}
	char *wvrt_next = wvrt_storage;
	// All copied strings remain live simultaneously until the native call returns.
	for (size_t wvrt_i = 0; wvrt_i < wvrtcount; ++wvrt_i) {
		wvrt_cstr_argument *wvrt_argument = &wvrtarguments[wvrt_i];
		const size_t wvrt_length = (size_t)wvrt_argument->wvrt_view.wvrt_length;
		if (wvrt_length != 0) { memcpy(wvrt_next, wvrt_argument->wvrt_view.wvrt_items, wvrt_length); }
		wvrt_next[wvrt_length] = '\0';
		wvrt_argument->wvrt_value = wvrt_next;
		wvrt_next += wvrt_length + 1;
	}
	return wvrt_storage;
}

// Release only the temporary storage owned by a completed native call.
void wvrt_cstr_drop(char *wvrtstorage) {
	free(wvrtstorage);
}

// Report a host argument conversion failure before calling the Wove entry.
static int wvrt_host_arguments_error(void) {
	fputs("runtime error: could not capture host arguments\n", stderr);
	return EXIT_FAILURE;
}

// Pack the user's original argument bytes and lend them and their spans to Wove.
int wvrt_run_host_entry(int argc, char **argv, size_t span_size,
	void (*write_span)(void *, int32_t, int32_t, int32_t), int32_t (*entry)(wvrt_slice, wvrt_slice)) {
	// Both lengths must fit in the runtime's signed slice domain.
	if (argc < 0 || (argc > 0 && (uintmax_t)(argc - 1) > INT32_MAX) || span_size == 0) {
		return wvrt_host_arguments_error();
	}
	const int32_t count = argc == 0 ? 0 : (int32_t)(argc - 1);
	// The host supplies only the executable name in the usual empty case.
	if (count == 0) {
		return entry((wvrt_slice){NULL, 0, sizeof(uint8_t)}, (wvrt_slice){NULL, 0, span_size});
	}
	// Check the span extent before allocating its record storage.
	if (argv == NULL || (uintmax_t)count > SIZE_MAX / span_size) {
		return wvrt_host_arguments_error();
	}
	size_t length = 0;
	// Measure the byte arena without counting the executable name or C terminators.
	for (int32_t index = 0; index < count; ++index) {
		if (argv[index + 1] == NULL) { return wvrt_host_arguments_error(); }
		const size_t width = strlen(argv[index + 1]);
		// Keep every half-open end representable as a Wove i32 and a host allocation offset.
		if ((uintmax_t)width > INT32_MAX - (uintmax_t)length || width > SIZE_MAX - length) {
			return wvrt_host_arguments_error();
		}
		length += width;
	}
	uint8_t *bytes = length == 0 ? NULL : malloc(length);
	void *spans = malloc((size_t)count * span_size);
	// Release whichever allocation succeeded when the other fails.
	if ((length > 0 && bytes == NULL) || spans == NULL) {
		free(spans);
		free(bytes);
		return wvrt_host_arguments_error();
	}
	size_t offset = 0;
	// Equal endpoints preserve an empty argument as a distinct row.
	for (int32_t index = 0; index < count; ++index) {
		const char *argument = argv[index + 1];
		const size_t width = strlen(argument);
		if (width > 0) { memcpy(bytes + offset, argument, width); }
		write_span(spans, index, (int32_t)offset, (int32_t)(offset + width));
		offset += width;
	}
	const int32_t status = entry((wvrt_slice){bytes, (int32_t)length, sizeof(uint8_t)},
		(wvrt_slice){spans, count, span_size});
	free(spans);
	free(bytes);
	return status;
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

// Allocate zero-initialized elements using their native layout and original Wove span.
wvrt_array *wvrt_array_new_at_span(int32_t wvrtlength, size_t wvrtelement_size, size_t wvrtelement_alignment,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	if (wvrtlength < 0) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "array length cannot be negative"); }
	size_t wvrt_count = wvrtlength == 0 ? 1 : (size_t)wvrtlength;
	// Reject an unrepresentable backing size before requesting storage.
	if (wvrt_count > SIZE_MAX / wvrtelement_size) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "array allocation size overflow");
	}
	wvrt_array *wvrt_result = wvrt_array_try_new(wvrtlength, wvrtelement_size, wvrtelement_alignment);
	if (wvrt_result == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "could not allocate array"); }
	return wvrt_result;
}

// Supply the native byte layout for the existing Stage 2 byte allocation ABI.
wvrt_array *wvrt_array_new_u8_at_span(int32_t wvrtlength, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_array_new_at_span(wvrtlength, sizeof(uint8_t), _Alignof(uint8_t), wvrtfile, wvrtstart, wvrtend);
}

// Copy compiled bytes into a fresh owner on each evaluation.
wvrt_array *wvrt_array_copy_u8_at_span(const void *wvrtbytes, int32_t wvrtlength,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_array *wvrt_result = wvrt_array_new_u8_at_span(wvrtlength, wvrtfile, wvrtstart, wvrtend);
	if (wvrtlength > 0) { memcpy(wvrt_result->wvrt_items, wvrtbytes, (size_t)wvrtlength); }
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

// Check the signed endpoints before deriving either the byte offset or the length.
static bool wvrt_slice_range_valid(int32_t wvrtlength, int32_t wvrtstart, int32_t wvrtend) {
	return 0 <= wvrtstart && wvrtstart <= wvrtend && wvrtend <= wvrtlength;
}

// Project an already checked byte range onto the same backing storage.
static wvrt_slice wvrt_slice_subrange(const void *wvrtitems, int32_t wvrtstart, int32_t wvrtend) {
	const uint8_t *wvrt_base = wvrtitems;
	if (wvrtstart != 0) { wvrt_base += (size_t)wvrtstart; }
	return (wvrt_slice){wvrt_base, wvrtend - wvrtstart, sizeof(uint8_t)};
}

// Form a shared view relative to the first byte and retain the seed's line diagnostic.
wvrt_slice wvrt_slice_range_u8(const void *wvrtitems, int32_t wvrtlength,
	int32_t wvrtstart, int32_t wvrtend, wvrt_location wvrtsite) {
	// Reject an invalid range before any pointer arithmetic.
	if (!wvrt_slice_range_valid(wvrtlength, wvrtstart, wvrtend)) {
		wvrt_trap(wvrtsite, "slice range out of bounds");
	}
	return wvrt_slice_subrange(wvrtitems, wvrtstart, wvrtend);
}

// Form a Stage 2 view with a diagnostic at the original range expression.
wvrt_slice wvrt_slice_range_u8_at_span(wvrt_slice wvrtview, int32_t wvrtstart, int32_t wvrtend,
	int32_t wvrtfile, int32_t wvrtspan_start, int32_t wvrtspan_end) {
	// Keep bounds failures ahead of pointer arithmetic and the callee.
	if (!wvrt_slice_range_valid(wvrtview.wvrt_length, wvrtstart, wvrtend)) {
		wvrt_trap_at_span(wvrtfile, wvrtspan_start, wvrtspan_end, "slice range out of bounds");
	}
	return wvrt_slice_subrange(wvrtview.wvrt_items, wvrtstart, wvrtend);
}

// Check an element index in the signed slice length domain.
static bool wvrt_slice_index_valid(int32_t wvrtlength, int32_t wvrtindex) {
	return 0 <= wvrtindex && wvrtindex < wvrtlength;
}

// Validate the signed index before converting it to an element offset.
size_t wvrt_slice_offset(int32_t wvrtlength, int32_t wvrtindex, wvrt_location wvrtsite) {
	// Negative indices and the one-past endpoint cannot name a byte.
	if (!wvrt_slice_index_valid(wvrtlength, wvrtindex)) {
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

// Check a Stage 2 index before deriving a backing storage address.
size_t wvrt_slice_offset_at_span(int32_t wvrtlength, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Preserve the span of the original access in the trap.
	if (!wvrt_slice_index_valid(wvrtlength, wvrtindex)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "slice index out of bounds");
	}
	return (size_t)wvrtindex;
}

// Check a fixed-array index against its compile-time extent before access.
size_t wvrt_array_index_at_span(int32_t wvrtlength, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// The original fixed-array access supplies the diagnostic span.
	if (!wvrt_slice_index_valid(wvrtlength, wvrtindex)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "array index out of bounds");
	}
	return (size_t)wvrtindex;
}

// Read a byte from the original storage through a checked Stage 2 view.
uint8_t wvrt_slice_load_u8_at_span(wvrt_slice wvrtview, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_offset = wvrt_slice_offset_at_span(wvrtview.wvrt_length, wvrtindex, wvrtfile, wvrtstart, wvrtend);
	return ((const uint8_t *)wvrtview.wvrt_items)[wvrt_offset];
}

// Update the original storage after checking the mutable view's byte index.
void wvrt_slice_store_u8(void *wvrtitems, int32_t wvrtlength, int32_t wvrtindex,
	uint8_t wvrtvalue, wvrt_location wvrtsite) {
	size_t wvrt_offset = wvrt_slice_offset(wvrtlength, wvrtindex, wvrtsite);
	((uint8_t *)wvrtitems)[wvrt_offset] = wvrtvalue;
}

// Store through the caller's mutable view only after checking the signed index.
void wvrt_slice_store_u8_at_span(wvrt_mut_slice wvrtview, int32_t wvrtindex, uint8_t wvrtvalue,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_offset = wvrt_slice_offset_at_span(wvrtview.wvrt_length, wvrtindex, wvrtfile, wvrtstart, wvrtend);
	((uint8_t *)wvrtview.wvrt_items)[wvrt_offset] = wvrtvalue;
}

// All supported integers fit in the common signed domain, including high-bit u32 values.
static bool wvrt_int_fits(int64_t wvrtvalue, int64_t wvrtmin, int64_t wvrtmax) {
	return wvrtmin <= wvrtvalue && wvrtvalue <= wvrtmax;
}

// Check an integer in the common signed domain before the caller casts it.
int64_t wvrt_int_convert(int64_t wvrtvalue, int64_t wvrtmin, int64_t wvrtmax,
	wvrt_location wvrtsite) {
	// Report Stage 1 failures in the seed's original filename and coordinate format.
	if (!wvrt_int_fits(wvrtvalue, wvrtmin, wvrtmax)) {
		wvrt_trap(wvrtsite, "integer conversion out of range");
	}
	return wvrtvalue;
}

// Report a Stage 2 integer conversion at its original span.
_Noreturn void wvrt_int_conversion_trap(int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer conversion out of range");
}

// Check Stage 2 conversions before the target-specific wrapper narrows the value.
static int64_t wvrt_int_at_span(int64_t wvrtvalue, int64_t wvrtmin, int64_t wvrtmax,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Source identity and offsets describe the conversion in its original file.
	if (!wvrt_int_fits(wvrtvalue, wvrtmin, wvrtmax)) {
		wvrt_int_conversion_trap(wvrtfile, wvrtstart, wvrtend);
	}
	return wvrtvalue;
}

// Narrow to an unsigned byte only after checking its exact limits.
uint8_t wvrt_int_to_u8(int64_t wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return (uint8_t)wvrt_int_at_span(wvrtvalue, 0, UINT8_MAX, wvrtfile, wvrtstart, wvrtend);
}

// Preserve all representable signed 32-bit values.
int32_t wvrt_int_to_i32(int64_t wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return (int32_t)wvrt_int_at_span(wvrtvalue, INT32_MIN, INT32_MAX, wvrtfile, wvrtstart, wvrtend);
}

// Check unsigned 32-bit bounds in the wider signed domain.
uint32_t wvrt_int_to_u32(int64_t wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return (uint32_t)wvrt_int_at_span(wvrtvalue, 0, UINT32_MAX, wvrtfile, wvrtstart, wvrtend);
}

// Reduce the unsigned sum to its low 32 bits.
uint32_t wvrt_u32_add_wrapping(uint32_t wvrtleft, uint32_t wvrtright) {
	return (uint32_t)((uint64_t)wvrtleft + wvrtright);
}

// Reduce the unsigned difference to its low 32 bits.
uint32_t wvrt_u32_sub_wrapping(uint32_t wvrtleft, uint32_t wvrtright) {
	return (uint32_t)((uint64_t)wvrtleft - wvrtright);
}

// Use the Wove operand width even when the C shift widens the word.
static bool wvrt_valid_u32_shift_count(int32_t wvrtcount) {
	return 0 <= wvrtcount && wvrtcount < 32;
}

// Validate a seed shift count before it reaches the C shift.
int32_t wvrt_u32_shift_count(int32_t wvrtcount, wvrt_location wvrtsite) {
	if (!wvrt_valid_u32_shift_count(wvrtcount)) { wvrt_trap(wvrtsite, "shift count out of range"); }
	return wvrtcount;
}

// Validate a native shift count and keep the original file and span.
int32_t wvrt_u32_shift_count_at_span(int32_t wvrtcount, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Both signed and width failures point at the full shift operation.
	if (!wvrt_valid_u32_shift_count(wvrtcount)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "shift count out of range");
	}
	return wvrtcount;
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

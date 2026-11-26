// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <errno.h>
#include <float.h>
#include <math.h>
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

// The native floating ABI uses IEEE binary32 and binary64 storage.
_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 &&
	sizeof(float) == sizeof(uint32_t), "Wove f32 requires native binary32");
_Static_assert(DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024 &&
	sizeof(double) == sizeof(uint64_t), "Wove f64 requires native binary64");

// Reinterpret a word as binary32 without changing its representation.
float wvrt_f32_from_bits(uint32_t wvrtbits) {
	float wvrtvalue;
	memcpy(&wvrtvalue, &wvrtbits, sizeof(wvrtvalue));
	return wvrtvalue;
}

// Copy an f32 sign, exponent, and payload to its exact-width integer.
uint32_t wvrt_f32_to_bits(float wvrtvalue) {
	uint32_t wvrtbits;
	memcpy(&wvrtbits, &wvrtvalue, sizeof(wvrtbits));
	return wvrtbits;
}

// Reinterpret a wide word as binary64 without changing its representation.
double wvrt_f64_from_bits(uint64_t wvrtbits) {
	double wvrtvalue;
	memcpy(&wvrtvalue, &wvrtbits, sizeof(wvrtvalue));
	return wvrtvalue;
}

// Copy an f64 sign, exponent, and payload to its exact-width integer.
uint64_t wvrt_f64_to_bits(double wvrtvalue) {
	uint64_t wvrtbits;
	memcpy(&wvrtbits, &wvrtvalue, sizeof(wvrtbits));
	return wvrtbits;
}

// Check the open interval before the generated integer cast truncates toward zero.
double wvrt_float_to_integer(double wvrtvalue, double wvrtlower, double wvrtupper,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Negating the ordered comparisons rejects NaN along with either overflow.
	if (!(wvrtvalue > wvrtlower && wvrtvalue < wvrtupper)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer conversion out of range");
	}
	return wvrtvalue;
}

// Keep IEEE nonfinite values and reject finite overflow when rounding to binary32.
float wvrt_f32_narrow(double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Subnormals and ordinary rounding are part of the destination's native behavior.
	if (isfinite(wvrtvalue) && (wvrtvalue < -FLT_MAX || wvrtvalue > FLT_MAX)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "floating conversion out of range");
	}
	return (float)wvrtvalue;
}

// Trap at the argument that failed to meet its callee's required slice length.
void wvrt_check_slice_extent_at_span(int32_t wvrt_length, int32_t wvrt_expected,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end) {
	// The actual descriptor and the declared extent count the same element type.
	if (wvrt_length != wvrt_expected) {
		wvrt_trap_at_span(wvrt_file, wvrt_start, wvrt_end, "array borrow length does not match parameter extent");
	}
}

// A borrowed native handle must be present before it is passed to C.
void wvrt_check_c_handle(const volatile void *wvrthandle, int32_t wvrtfile,
	int32_t wvrtstart, int32_t wvrtend) {
	if (wvrthandle == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "missing C handle"); }
}

// Guard the pointer-out slot before C can overwrite a live owned handle.
void wvrt_check_c_handle_empty(const volatile void *wvrt_handle, int32_t wvrt_file,
	int32_t wvrt_start, int32_t wvrt_end) {
	if (wvrt_handle != NULL) { wvrt_trap_at_span(wvrt_file, wvrt_start, wvrt_end, "C handle is already initialized"); }
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

// Validate a native array's element count using the original borrowed descriptor.
void wvrt_check_c_array(wvrt_slice wvrtview, size_t wvrtelement_size, int64_t wvrtcount,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Reject a negative element count before checking the view.
	if (wvrtcount < 0) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "negative C array extent");
	}
	// Empty views need no backing address. Other views must have the declared element width.
	if (wvrtview.wvrt_length < 0 || (wvrtview.wvrt_length > 0 &&
		(wvrtview.wvrt_items == NULL || wvrtview.wvrt_element_size != wvrtelement_size))) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "invalid C array slice");
	}
	// Compare in elements before C receives the backing address.
	if (wvrtcount > wvrtview.wvrt_length) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "C array extent out of bounds");
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

// Restore only the original view bytes and keep the native call's errno.
void wvrt_cstr_copy_back(wvrt_mut_slice wvrtview, const char *wvrtvalue) {
	if (wvrtview.wvrt_length == 0) { return; }
	const int wvrt_saved_errno = errno;
	memcpy(wvrtview.wvrt_items, wvrtvalue, (size_t)wvrtview.wvrt_length);
	errno = wvrt_saved_errno;
}

// Release the call's temporary storage while retaining the native call's errno.
void wvrt_cstr_drop(char *wvrtstorage) {
	const int wvrt_saved_errno = errno;
	free(wvrtstorage);
	errno = wvrt_saved_errno;
}

// Copy byte rows into one call-scoped allocation for the table and terminated payloads.
char *const *wvrt_cstrs_prepare(wvrt_slice wvrtrows, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Require the exact row descriptor before interpreting its backing storage.
	if (wvrtrows.wvrt_length < 0 || wvrtrows.wvrt_element_size != sizeof(wvrt_array *) ||
		(wvrtrows.wvrt_length > 0 && wvrtrows.wvrt_items == NULL)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "invalid C string vector");
	}
	const size_t wvrt_count = (size_t)wvrtrows.wvrt_length;
	// Reserve one pointer for the terminating null, including for a zero-row vector.
	if (wvrt_count >= SIZE_MAX / sizeof(char *)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "C string vector allocation size overflow");
	}
	const size_t wvrt_table_size = (wvrt_count + 1) * sizeof(char *);
	size_t wvrt_size = wvrt_table_size;
	wvrt_array *const *wvrt_rows = wvrtrows.wvrt_items;
	// Validate every row and measure its full terminated size before allocating.
	for (size_t wvrt_i = 0; wvrt_i < wvrt_count; ++wvrt_i) {
		const wvrt_array *wvrt_row = wvrt_rows[wvrt_i];
		// An unfilled row has no byte storage, unlike an owned zero-length row.
		if (wvrt_row == NULL) {
			wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "uninitialized byte row");
		}
		const size_t wvrt_length = (size_t)wvrt_row->wvrt_length;
		// Native C strings cannot preserve an embedded NUL.
		if (wvrt_length > 0 && memchr(wvrt_row->wvrt_items, 0, wvrt_length) != NULL) {
			wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "embedded NUL in C string vector argument");
		}
		// Include the trailing NUL for each row in the total allocation size.
		if (wvrt_length >= SIZE_MAX - wvrt_size) {
			wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "C string vector allocation size overflow");
		}
		wvrt_size += wvrt_length + 1;
	}
	char **wvrt_table = malloc(wvrt_size);
	if (wvrt_table == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "could not allocate C string vector"); }
	char *wvrt_payload = (char *)wvrt_table + wvrt_table_size;
	// Keep each copied row distinct, including an empty row's terminating byte.
	for (size_t wvrt_i = 0; wvrt_i < wvrt_count; ++wvrt_i) {
		const wvrt_array *wvrt_row = wvrt_rows[wvrt_i];
		const size_t wvrt_length = (size_t)wvrt_row->wvrt_length;
		wvrt_table[wvrt_i] = wvrt_payload;
		if (wvrt_length > 0) { memcpy(wvrt_payload, wvrt_row->wvrt_items, wvrt_length); }
		wvrt_payload[wvrt_length] = '\0';
		wvrt_payload += wvrt_length + 1;
	}
	wvrt_table[wvrt_count] = NULL;
	return wvrt_table;
}

// Release a generated table and its adjacent bytes while preserving the native errno.
void wvrt_cstrs_drop(char *const *wvrt_table) {
	wvrt_cstr_drop((char *)wvrt_table);
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

// Keep native object storage stable until the Wove owner is released.
struct wvrt_c_storage {
	void *wvrt_data;
	_Bool wvrt_active;
};

// Allocate an inactive owner and zeroed storage at the actual C type's alignment.
wvrt_c_storage *wvrt_c_storage_new(size_t wvrt_size, size_t wvrt_alignment,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end) {
	wvrt_c_storage *wvrt_storage = malloc(sizeof *wvrt_storage);
	// A complete native object has a nonzero size and alignment.
	if (wvrt_storage == NULL || wvrt_size == 0 || wvrt_alignment == 0) {
		free(wvrt_storage);
		wvrt_trap_at_span(wvrt_file, wvrt_start, wvrt_end, "could not allocate C storage");
	}
	wvrt_storage->wvrt_data = wvrt_calloc(1, wvrt_size, wvrt_alignment);
	// Nothing owns the raw allocation if the constructor cannot finish.
	if (wvrt_storage->wvrt_data == NULL) {
		free(wvrt_storage);
		wvrt_trap_at_span(wvrt_file, wvrt_start, wvrt_end, "could not allocate C storage");
	}
	wvrt_storage->wvrt_active = 0;
	return wvrt_storage;
}

// Require inactive storage for initialization and an active object for other native calls.
void wvrt_c_storage_access(wvrt_c_storage *wvrt_storage, _Bool wvrt_initializing,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end) {
	// Check the owner before accessing its lifetime state.
	if (wvrt_storage == NULL) {
		wvrt_trap_at_span(wvrt_file, wvrt_start, wvrt_end, "missing C storage");
	}
	// The initialization status controls which operations may take its address.
	if (wvrt_storage->wvrt_active == wvrt_initializing) {
		const char *wvrt_problem = wvrt_initializing ? "C storage is already initialized" : "C storage is not initialized";
		wvrt_trap_at_span(wvrt_file, wvrt_start, wvrt_end, wvrt_problem);
	}
}

// Return the object address after a checked loan or for its active destructor.
void *wvrt_c_storage_data(wvrt_c_storage *wvrt_storage) {
	return wvrt_storage->wvrt_data;
}

// Parent scope cleanup ignores inactive and already consumed owners.
_Bool wvrt_c_storage_active(const wvrt_c_storage *wvrt_storage) {
	return wvrt_storage != NULL && wvrt_storage->wvrt_active;
}

// A failed C initializer leaves the storage inactive and eligible for another attempt.
void wvrt_c_storage_finish_init(wvrt_c_storage *wvrt_storage, int32_t wvrt_status) {
	wvrt_storage->wvrt_active = wvrt_status == 0;
}

// Release the wrapper and its backing allocation after any required C destructor.
void wvrt_c_storage_free(wvrt_c_storage *wvrt_storage) {
	if (wvrt_storage == NULL) { return; }
	int wvrt_error = errno;
	free(wvrt_storage->wvrt_data);
	free(wvrt_storage);
	errno = wvrt_error;
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

// Release the allocation stored in one owning row slot, including an empty slot.
static void wvrt_drop_row(void *wvrt_slot) {
	wvrt_array_drop(*(wvrt_array **)wvrt_slot);
}

// Allocate initialized pointer slots and register their recursive cleanup.
wvrt_array *wvrt_array_new_rows_at_span(int32_t wvrtlength, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_array *wvrt_rows = wvrt_array_new_at_span(wvrtlength, sizeof(wvrt_array *), _Alignof(wvrt_array *),
		wvrtfile, wvrtstart, wvrtend);
	wvrt_array **wvrt_items = wvrt_rows->wvrt_items;
	// Give each slot a typed null pointer before any replacement or cleanup.
	for (int32_t wvrt_i = 0; wvrt_i < wvrtlength; ++wvrt_i) {
		wvrt_items[wvrt_i] = NULL;
	}
	wvrt_rows->wvrt_drop_element = wvrt_drop_row;
	return wvrt_rows;
}

// Lend an initialized row without transferring its owner.
wvrt_slice wvrt_array_borrow_row_at_span(wvrt_slice wvrtrows, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_offset = wvrt_slice_offset_at_span(wvrtrows.wvrt_length, wvrtindex, wvrtfile, wvrtstart, wvrtend);
	wvrt_array *const *wvrt_items = wvrtrows.wvrt_items;
	const wvrt_array *wvrt_row = wvrt_items[wvrt_offset];
	// Unfilled slots have no borrowable byte storage.
	if (wvrt_row == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "uninitialized byte row"); }
	return wvrt_array_borrow_shared(wvrt_row);
}

// Transfer one fresh or moved row and retire the owner that previously occupied the slot.
void wvrt_array_store_row_at_span(wvrt_mut_slice wvrtrows, int32_t wvrtindex, wvrt_array *wvrtrow,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_offset = wvrt_slice_offset_at_span(wvrtrows.wvrt_length, wvrtindex, wvrtfile, wvrtstart, wvrtend);
	wvrt_array **wvrt_items = wvrtrows.wvrt_items;
	wvrt_array_drop(wvrt_items[wvrt_offset]);
	wvrt_items[wvrt_offset] = wvrtrow;
}

// Copy compiled bytes into a fresh owner on each evaluation.
wvrt_array *wvrt_array_copy_u8_at_span(const void *wvrtbytes, int32_t wvrtlength,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_array *wvrt_result = wvrt_array_new_u8_at_span(wvrtlength, wvrtfile, wvrtstart, wvrtend);
	if (wvrtlength > 0) { memcpy(wvrt_result->wvrt_items, wvrtbytes, (size_t)wvrtlength); }
	return wvrt_result;
}

// Copy a measured native string into an independent owner, including an empty string.
static wvrt_array *wvrt_try_copy_cstr_bytes(const char *wvrttext, size_t wvrtlength) {
	if (wvrtlength > INT32_MAX) { return NULL; }
	wvrt_array *wvrt_result = wvrt_array_try_new((int32_t)wvrtlength, sizeof(uint8_t), _Alignof(uint8_t));
	if (wvrt_result != NULL && wvrtlength != 0) { memcpy(wvrt_result->wvrt_items, wvrttext, wvrtlength); }
	return wvrt_result;
}

// Measure a null-terminated native string before copying it.
static wvrt_array *wvrt_try_copy_cstr(const char *wvrttext) {
	return wvrt_try_copy_cstr_bytes(wvrttext, strlen(wvrttext));
}

// Report snapshot failure after releasing owned native storage, or restore the call's errno.
static wvrt_array *wvrt_finish_cstr_snapshot(wvrt_array *wvrtresult, int wvrtsaved_errno,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	if (wvrtresult == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "could not snapshot C string"); }
	errno = wvrtsaved_errno;
	return wvrtresult;
}

// Copy a borrowed result while its C backing remains live.
wvrt_array *wvrt_snapshot_borrowed_cstr_at_span(const char *wvrttext,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	if (wvrttext == NULL) { return NULL; }
	const int wvrt_saved_errno = errno;
	wvrt_array *wvrt_result = wvrt_try_copy_cstr(wvrttext);
	return wvrt_finish_cstr_snapshot(wvrt_result, wvrt_saved_errno, wvrtfile, wvrtstart, wvrtend);
}

// Scan only through the first terminator or the declared inline field capacity.
wvrt_array *wvrt_snapshot_bounded_cstr_at_span(const char *wvrttext, size_t wvrtcapacity,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	const int wvrt_saved_errno = errno;
	size_t wvrt_length = 0;
	// Test the bound before inspecting the next byte.
	while (wvrt_length < wvrtcapacity && wvrttext[wvrt_length] != '\0') {
		++wvrt_length;
	}
	// A full field without a terminator cannot supply a C string.
	if (wvrt_length == wvrtcapacity) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "unterminated C snapshot field");
	}
	wvrt_array *wvrt_result = wvrt_try_copy_cstr_bytes(wvrttext, wvrt_length);
	return wvrt_finish_cstr_snapshot(wvrt_result, wvrt_saved_errno, wvrtfile, wvrtstart, wvrtend);
}

// Copy the native text before invoking its release, including on copy failure.
static wvrt_array *wvrt_snapshot_cstr(char *wvrttext, void (*wvrtrelease)(void *),
	void (*wvrtchars_release)(char *), int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	if (wvrttext == NULL) { return NULL; }
	const int wvrt_saved_errno = errno;
	wvrt_array *wvrt_result = wvrt_try_copy_cstr(wvrttext);
	// The checked bridge selects the matching release signature without a cast.
	if (wvrtrelease != NULL) { wvrtrelease(wvrttext); }
	else { wvrtchars_release(wvrttext); }
	return wvrt_finish_cstr_snapshot(wvrt_result, wvrt_saved_errno, wvrtfile, wvrtstart, wvrtend);
}

// Accept the standard free signature for an allocating C string.
wvrt_array *wvrt_snapshot_cstr_at_span(char *wvrttext, void (*wvrtrelease)(void *),
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_snapshot_cstr(wvrttext, wvrtrelease, NULL, wvrtfile, wvrtstart, wvrtend);
}

// Accept a release function that takes the exact mutable character pointer.
wvrt_array *wvrt_snapshot_chars_at_span(char *wvrttext, void (*wvrtrelease)(char *),
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_snapshot_cstr(wvrttext, NULL, wvrtrelease, wvrtfile, wvrtstart, wvrtend);
}

// Build an independent row owner for each element of the borrowed, terminated native table.
wvrt_array *wvrt_snapshot_cstrs_at_span(char *const *wvrttable, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	const int wvrt_saved_errno = errno;
	int32_t wvrt_count = 0;
	// A missing table represents an empty Wove vector.
	while (wvrttable != NULL && wvrttable[wvrt_count] != NULL) {
		// Keep row counts representable by Wove's array descriptor.
		if (wvrt_count == INT32_MAX) {
			wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "C string snapshot vector is too long");
		}
		++wvrt_count;
	}
	wvrt_array *wvrt_rows = wvrt_array_new_rows_at_span(wvrt_count, wvrtfile, wvrtstart, wvrtend);
	wvrt_array **wvrt_items = wvrt_rows->wvrt_items;
	// Leave a safely droppable prefix if a row cannot be copied.
	for (int32_t wvrt_i = 0; wvrt_i < wvrt_count; ++wvrt_i) {
		wvrt_array *wvrt_row = wvrt_try_copy_cstr(wvrttable[wvrt_i]);
		// Only the Wove prefix is owned and must be released on failure.
		if (wvrt_row == NULL) {
			wvrt_array_drop(wvrt_rows);
			wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "could not snapshot C string vector row");
		}
		wvrt_items[wvrt_i] = wvrt_row;
	}
	errno = wvrt_saved_errno;
	return wvrt_rows;
}

// Extract the same Wove allocation after checking a snapshot's presence.
wvrt_array *wvrt_require_array_at_span(wvrt_array *wvrtvalue, int32_t wvrtfile,
	int32_t wvrtstart, int32_t wvrtend) {
	if (wvrtvalue == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "missing optional array"); }
	return wvrtvalue;
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

// Publish new capacity only after aligned allocation and copying the live prefix succeed.
bool wvrt_array_try_reserve(wvrt_array *wvrtbuffer, int32_t wvrtcapacity) {
	if (wvrtcapacity < 0) { return false; }
	if (wvrtcapacity <= wvrtbuffer->wvrt_capacity) { return true; }
	size_t wvrt_size = wvrtbuffer->wvrt_element_size;
	void *wvrt_storage = wvrt_calloc((size_t)wvrtcapacity, wvrt_size,
		wvrtbuffer->wvrt_element_alignment);
	if (wvrt_storage == NULL) { return false; }
	memcpy(wvrt_storage, wvrtbuffer->wvrt_items, (size_t)wvrtbuffer->wvrt_length * wvrt_size);
	free(wvrtbuffer->wvrt_items);
	wvrtbuffer->wvrt_items = wvrt_storage;
	wvrtbuffer->wvrt_capacity = wvrtcapacity;
	wvrtbuffer->wvrt_spare = 0;
	return true;
}

// Trap at the written reserve operation if its requested capacity cannot be provided.
void wvrt_array_reserve_at_span(wvrt_array *wvrtbuffer, int32_t wvrtcapacity,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Distinguish a negative requested capacity from an allocation failure.
	if (wvrtcapacity < 0) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "array capacity cannot be negative");
	}
	// The checked reserve leaves the original descriptor intact on failure.
	if (!wvrt_array_try_reserve(wvrtbuffer, wvrtcapacity)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "could not grow array");
	}
}

// Append a value captured outside the array, growing geometrically when needed.
void wvrt_array_push_at_span(wvrt_array *wvrtbuffer, const void *wvrtvalue,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	int32_t wvrt_length = wvrtbuffer->wvrt_length;
	// Check the largest representable length before adding the new element.
	if (wvrt_length == INT32_MAX) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "array length overflow");
	}
	// Only allocate when the live prefix has filled the current storage.
	if (wvrt_length == wvrtbuffer->wvrt_capacity) {
		int32_t wvrt_capacity = wvrtbuffer->wvrt_capacity;
		int32_t wvrt_next = wvrt_capacity > INT32_MAX / 2 ? INT32_MAX : wvrt_capacity * 2;
		if (wvrt_next <= wvrt_length) { wvrt_next = wvrt_length + 1; }
		wvrt_array_reserve_at_span(wvrtbuffer, wvrt_next, wvrtfile, wvrtstart, wvrtend);
	}
	size_t wvrt_size = wvrtbuffer->wvrt_element_size;
	memcpy((unsigned char *)wvrtbuffer->wvrt_items + (size_t)wvrt_length * wvrt_size,
		wvrtvalue, wvrt_size);
	wvrtbuffer->wvrt_length = wvrt_length + 1;
	wvrtbuffer->wvrt_spare = 0;
}

// Close an occupied slot without dropping owners moved into the surviving prefix.
static void wvrt_array_erase(wvrt_array *wvrtbuffer, size_t wvrtindex) {
	size_t wvrt_size = wvrtbuffer->wvrt_element_size;
	size_t wvrt_tail = (size_t)wvrtbuffer->wvrt_length - wvrtindex - 1;
	// The final slot falls outside the live prefix and is no longer owned by the array.
	if (wvrt_tail != 0) {
		unsigned char *wvrt_item = (unsigned char *)wvrtbuffer->wvrt_items + wvrtindex * wvrt_size;
		memmove(wvrt_item, wvrt_item + wvrt_size, wvrt_tail * wvrt_size);
	}
	--wvrtbuffer->wvrt_length;
	wvrtbuffer->wvrt_spare = 0;
}

// Copy the removed element into its result before shifting the tail.
void wvrt_array_remove_at_span(wvrt_array *wvrtbuffer, int32_t wvrtindex, void *wvrtresult,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_index = wvrt_array_index_at_span(wvrtbuffer->wvrt_length, wvrtindex,
		wvrtfile, wvrtstart, wvrtend);
	size_t wvrt_size = wvrtbuffer->wvrt_element_size;
	const unsigned char *wvrt_item = (const unsigned char *)wvrtbuffer->wvrt_items + wvrt_index * wvrt_size;
	memcpy(wvrtresult, wvrt_item, wvrt_size);
	wvrt_array_erase(wvrtbuffer, wvrt_index);
}

// Transfer a populated row to its new owner. An unfilled slot has no owner to return.
wvrt_array *wvrt_array_remove_row_at_span(wvrt_array *wvrtbuffer, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_index = wvrt_array_index_at_span(wvrtbuffer->wvrt_length, wvrtindex,
		wvrtfile, wvrtstart, wvrtend);
	wvrt_array *wvrt_row = ((wvrt_array **)wvrtbuffer->wvrt_items)[wvrt_index];
	if (wvrt_row == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "uninitialized byte row"); }
	wvrt_array_erase(wvrtbuffer, wvrt_index);
	return wvrt_row;
}

// Retire the live suffix while retaining the original allocation for reuse.
void wvrt_array_truncate_at_span(wvrt_array *wvrtbuffer, int32_t wvrtlength,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Only a prefix of the current live extent can remain after truncation.
	if (wvrtlength < 0 || wvrtlength > wvrtbuffer->wvrt_length) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "array truncation out of bounds");
	}
	wvrt_array_drop_range(wvrtbuffer, wvrtlength, wvrtbuffer->wvrt_length);
	wvrtbuffer->wvrt_length = wvrtlength;
	wvrtbuffer->wvrt_spare = 0;
}

// Check the signed endpoints before deriving either the byte offset or the length.
static bool wvrt_slice_range_valid(int32_t wvrtlength, int32_t wvrtstart, int32_t wvrtend) {
	return 0 <= wvrtstart && wvrtstart <= wvrtend && wvrtend <= wvrtlength;
}

// Project checked element bounds onto a shared view's original storage.
static wvrt_slice wvrt_slice_subrange(wvrt_slice wvrtview, int32_t wvrtstart, int32_t wvrtend) {
	const unsigned char *wvrt_base = wvrtview.wvrt_items;
	if (wvrtstart != 0) { wvrt_base += (size_t)wvrtstart * wvrtview.wvrt_element_size; }
	return (wvrt_slice){wvrt_base, wvrtend - wvrtstart, wvrtview.wvrt_element_size};
}

// Form a shared view relative to the first byte and retain the seed's line diagnostic.
wvrt_slice wvrt_slice_range_u8(const void *wvrtitems, int32_t wvrtlength,
	int32_t wvrtstart, int32_t wvrtend, wvrt_location wvrtsite) {
	// Reject an invalid range before any pointer arithmetic.
	if (!wvrt_slice_range_valid(wvrtlength, wvrtstart, wvrtend)) {
		wvrt_trap(wvrtsite, "slice range out of bounds");
	}
	return wvrt_slice_subrange((wvrt_slice){wvrtitems, wvrtlength, sizeof(uint8_t)}, wvrtstart, wvrtend);
}

// Form a shared element range with a diagnostic at the original range expression.
wvrt_slice wvrt_slice_range_at_span(wvrt_slice wvrtview, int32_t wvrtstart, int32_t wvrtend,
	int32_t wvrtfile, int32_t wvrtspan_start, int32_t wvrtspan_end) {
	// Keep bounds failures ahead of pointer arithmetic and the callee.
	if (!wvrt_slice_range_valid(wvrtview.wvrt_length, wvrtstart, wvrtend)) {
		wvrt_trap_at_span(wvrtfile, wvrtspan_start, wvrtspan_end, "slice range out of bounds");
	}
	return wvrt_slice_subrange(wvrtview, wvrtstart, wvrtend);
}

// Preserve the byte-range ABI and its unit element stride.
wvrt_slice wvrt_slice_range_u8_at_span(wvrt_slice wvrtview, int32_t wvrtstart, int32_t wvrtend,
	int32_t wvrtfile, int32_t wvrtspan_start, int32_t wvrtspan_end) {
	wvrtview.wvrt_element_size = sizeof(uint8_t);
	return wvrt_slice_range_at_span(wvrtview, wvrtstart, wvrtend, wvrtfile, wvrtspan_start, wvrtspan_end);
}

// Project checked element bounds onto a writable view's original storage.
static wvrt_mut_slice wvrt_mut_slice_subrange(wvrt_mut_slice wvrtview, int32_t wvrtstart, int32_t wvrtend) {
	unsigned char *wvrt_base = wvrtview.wvrt_items;
	if (wvrtstart != 0) { wvrt_base += (size_t)wvrtstart * wvrtview.wvrt_element_size; }
	return (wvrt_mut_slice){wvrt_base, wvrtend - wvrtstart, wvrtview.wvrt_element_size};
}

// Form a seed mutable view with a diagnostic at the range's original line.
wvrt_mut_slice wvrt_mut_slice_range_u8(void *wvrtitems, int32_t wvrtlength,
	int32_t wvrtstart, int32_t wvrtend, wvrt_location wvrtsite) {
	// Do not derive an address before both endpoints have passed the bounds check.
	if (!wvrt_slice_range_valid(wvrtlength, wvrtstart, wvrtend)) {
		wvrt_trap(wvrtsite, "slice range out of bounds");
	}
	return wvrt_mut_slice_subrange((wvrt_mut_slice){wvrtitems, wvrtlength, sizeof(uint8_t)}, wvrtstart, wvrtend);
}

// Form a writable element range with the original source span.
wvrt_mut_slice wvrt_mut_slice_range_at_span(wvrt_mut_slice wvrtview, int32_t wvrtstart, int32_t wvrtend,
	int32_t wvrtfile, int32_t wvrtspan_start, int32_t wvrtspan_end) {
	// Bounds failures precede pointer arithmetic and entry into the callee.
	if (!wvrt_slice_range_valid(wvrtview.wvrt_length, wvrtstart, wvrtend)) {
		wvrt_trap_at_span(wvrtfile, wvrtspan_start, wvrtspan_end, "slice range out of bounds");
	}
	return wvrt_mut_slice_subrange(wvrtview, wvrtstart, wvrtend);
}

// Preserve the mutable byte-range ABI and its unit element stride.
wvrt_mut_slice wvrt_mut_slice_range_u8_at_span(wvrt_mut_slice wvrtview, int32_t wvrtstart, int32_t wvrtend,
	int32_t wvrtfile, int32_t wvrtspan_start, int32_t wvrtspan_end) {
	wvrtview.wvrt_element_size = sizeof(uint8_t);
	return wvrt_mut_slice_range_at_span(wvrtview, wvrtstart, wvrtend, wvrtfile, wvrtspan_start, wvrtspan_end);
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

// The seed and small built-in conversions check bounds in a common signed domain.
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

// Assemble an exact-width byte view using unsigned arithmetic and unaligned byte reads.
uint64_t wvrt_int_from_bytes(wvrt_slice wvrtview, int32_t wvrtwidth, bool wvrtbigendian,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Check the extent before touching the first byte of the view.
	if (wvrtwidth < 1 || wvrtwidth > 8 || wvrtview.wvrt_length != wvrtwidth ||
		wvrtview.wvrt_element_size != sizeof(uint8_t) || wvrtview.wvrt_items == NULL) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer byte width mismatch");
	}
	const uint8_t *wvrtbytes = wvrtview.wvrt_items;
	uint64_t wvrtbits = 0;
	// Read the most significant byte first in either requested order.
	for (int32_t wvrtindex = 0; wvrtindex < wvrtwidth; ++wvrtindex) {
		int32_t wvrtoffset = wvrtbigendian ? wvrtindex : wvrtwidth - 1 - wvrtindex;
		wvrtbits = (wvrtbits << 8) | wvrtbytes[wvrtoffset];
	}
	return wvrtbits;
}

// Interpret two's complement without an out-of-range unsigned-to-signed cast.
int64_t wvrt_signed_from_bytes(wvrt_slice wvrtview, int32_t wvrtwidth, bool wvrtbigendian,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	uint64_t wvrtbits = wvrt_int_from_bytes(wvrtview, wvrtwidth, wvrtbigendian, wvrtfile, wvrtstart, wvrtend);
	uint64_t wvrtsign = UINT64_C(1) << (wvrtwidth * 8 - 1);
	// Positive encodings already fit the signed result type.
	if (wvrtbits < wvrtsign) {
		return (int64_t)wvrtbits;
	}
	return -INT64_C(1) - (int64_t)((wvrtsign - 1) - (wvrtbits - wvrtsign));
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
static bool wvrt_valid_shift_count(int32_t wvrtcount, int32_t width) {
	return 0 <= wvrtcount && wvrtcount < width;
}

// Validate a seed shift count before it reaches the C shift.
int32_t wvrt_u32_shift_count(int32_t wvrtcount, wvrt_location wvrtsite) {
	if (!wvrt_valid_shift_count(wvrtcount, 32)) { wvrt_trap(wvrtsite, "shift count out of range"); }
	return wvrtcount;
}

// Validate a native shift count and keep the original file and span.
static int32_t wvrt_shift_count_at_span(int32_t wvrtcount, int32_t width,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Both signed and width failures point at the full shift operation.
	if (!wvrt_valid_shift_count(wvrtcount, width)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "shift count out of range");
	}
	return wvrtcount;
}

// Check a native 32-bit word's shift count.
int32_t wvrt_u32_shift_count_at_span(int32_t wvrtcount, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_shift_count_at_span(wvrtcount, 32, wvrtfile, wvrtstart, wvrtend);
}

// Check a native 64-bit word's shift count.
int32_t wvrt_u64_shift_count_at_span(int32_t wvrtcount, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_shift_count_at_span(wvrtcount, 64, wvrtfile, wvrtstart, wvrtend);
}

// Interpret wrapped bits as a signed value using only in-range signed casts.
int32_t wvrt_i32_from_wrapped_bits(uint32_t wvrtbits) {
	if (wvrtbits <= INT32_MAX) { return (int32_t)wvrtbits; }
	return -1 - (int32_t)(UINT32_MAX - wvrtbits);
}

// Interpret a full two's-complement word using only in-range signed casts.
int64_t wvrt_i64_from_wrapped_bits(uint64_t wvrtbits) {
	if (wvrtbits <= (uint64_t)INT64_MAX) { return (int64_t)wvrtbits; }
	return -INT64_C(1) - (int64_t)(UINT64_MAX - wvrtbits);
}

// Shift a signed 32-bit word using a checked count and unsigned bit operations.
int32_t wvrt_i32_shift_at_span(int32_t wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrtcount = wvrt_shift_count_at_span(wvrtcount, 32, wvrtfile, wvrtstart, wvrtend);
	// Retain exactly the low 32 bits of a left shift.
	if (!wvrtright) {
		uint32_t bits = (uint32_t)((uint64_t)(uint32_t)wvrtvalue << wvrtcount);
		return wvrt_i32_from_wrapped_bits(bits);
	}
	// Shift the complemented magnitude to extend the sign of a negative word.
	if (wvrtvalue < 0) {
		uint32_t inverted = (uint32_t)(-1 - wvrtvalue);
		return -1 - (int32_t)(inverted >> wvrtcount);
	}
	return (int32_t)((uint32_t)wvrtvalue >> wvrtcount);
}

// Add in the modulo domain and return the corresponding signed value.
int32_t wvrt_i32_add_wrapping(int32_t wvrtleft, int32_t wvrtright) {
	return wvrt_i32_from_wrapped_bits(wvrt_u32_add_wrapping((uint32_t)wvrtleft, (uint32_t)wvrtright));
}

// Subtract in the modulo domain and return the corresponding signed value.
int32_t wvrt_i32_sub_wrapping(int32_t wvrtleft, int32_t wvrtright) {
	return wvrt_i32_from_wrapped_bits(wvrt_u32_sub_wrapping((uint32_t)wvrtleft, (uint32_t)wvrtright));
}

// Compute a signed quotient or remainder only after checking the Wove range.
int32_t wvrt_i32_divide_at_span(int32_t wvrtleft, int32_t wvrtright, bool wvrtremainder,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// A zero divisor is invalid for both operations.
	if (wvrtright == 0) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "division by zero");
	}
	// Handle the signed minimum without invoking either C operator on this pair.
	if (wvrtleft == INT32_MIN && wvrtright == -1) {
		if (wvrtremainder) { return 0; }
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer arithmetic overflow");
	}
	return wvrtremainder ? wvrtleft % wvrtright : wvrtleft / wvrtright;
}

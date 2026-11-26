// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keep the runtime usable on targets whose C library has no POSIX threads.
#if (defined(__unix__) || defined(__APPLE__)) && __has_include(<pthread.h>)
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#define WVRT_POSIX_WORKERS 1
#endif

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

// Targets without native half support omit the binary16 ABI.
#ifdef WVRT_HAS_F16
_Static_assert(__FLT16_MAX_EXP__ == 16 && sizeof(_Float16) == sizeof(uint16_t),
	"Wove f16 requires native binary16");

// Reinterpret a half word without changing its representation.
_Float16 wvrt_f16_from_bits(uint16_t wvrtbits) {
	_Float16 wvrtvalue;
	memcpy(&wvrtvalue, &wvrtbits, sizeof(wvrtvalue));
	return wvrtvalue;
}

// Copy the complete binary16 representation, including signed zero and NaN payloads.
uint16_t wvrt_f16_to_bits(_Float16 wvrtvalue) {
	uint16_t wvrtbits;
	memcpy(&wvrtbits, &wvrtvalue, sizeof(wvrtbits));
	return wvrtbits;
}

// Keep nonfinite inputs and reject finite values beyond the binary16 range.
_Float16 wvrt_f16_narrow(double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Clang rounds finite in-range values directly from double to half.
	if (isfinite(wvrtvalue) && (wvrtvalue < -65504.0 || wvrtvalue > 65504.0)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "floating conversion out of range");
	}
	return (_Float16)wvrtvalue;
}
#endif

// Targets with x87 extended precision retain all 64 significand bits during conversion checks.
#ifdef WVRT_HAS_F80
// Check the original extended value before truncating to an integer.
long double wvrt_f80_to_integer(long double wvrtvalue, long double wvrtlower, long double wvrtupper,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Ordered comparisons reject NaN as well as values outside the open interval.
	if (!(wvrtvalue > wvrtlower && wvrtvalue < wvrtupper)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer conversion out of range");
	}
	return wvrtvalue;
}

// Keep nonfinite values and compare finite inputs at their original precision.
static void wvrt_check_f80_range(long double wvrtvalue, long double wvrtlimit,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// The limit is the destination's exact largest finite magnitude.
	if (isfinite(wvrtvalue) && (wvrtvalue < -wvrtlimit || wvrtvalue > wvrtlimit)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "floating conversion out of range");
	}
}

// Round directly from extended precision to binary64 after checking its finite range.
double wvrt_f80_to_f64(long double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_check_f80_range(wvrtvalue, DBL_MAX, wvrtfile, wvrtstart, wvrtend);
	return (double)wvrtvalue;
}

// Round directly from extended precision to binary32 after checking its finite range.
float wvrt_f80_to_f32(long double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_check_f80_range(wvrtvalue, FLT_MAX, wvrtfile, wvrtstart, wvrtend);
	return (float)wvrtvalue;
}

// Compile the binary16 destination helper only when both ABIs are available.
#ifdef WVRT_HAS_F16
// Round directly from extended precision to binary16 after checking its finite range.
_Float16 wvrt_f80_to_f16(long double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_check_f80_range(wvrtvalue, 65504.0L, wvrtfile, wvrtstart, wvrtend);
	return (_Float16)wvrtvalue;
}
#endif
#endif

// Both native binary128 representations use the same checked conversion rules.
#ifdef WVRT_HAS_F128
// Check the open truncation interval before converting the original binary128 value.
wvrt_f128 wvrt_f128_to_integer(wvrt_f128 wvrtvalue, wvrt_f128 wvrtlower, wvrt_f128 wvrtupper,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Ordered comparisons reject NaNs and both ends of the overflow interval.
	if (!(wvrtvalue > wvrtlower && wvrtvalue < wvrtupper)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer conversion out of range");
	}
	return wvrtvalue;
}

// Classify and compare at binary128 precision even when long double has fewer bits.
static void wvrt_check_f128_range(wvrt_f128 wvrtvalue, wvrt_f128 wvrtlimit,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Nonfinite inputs keep their IEEE behavior on conversion to a narrower format.
	if (__builtin_isfinite(wvrtvalue) && (wvrtvalue < -wvrtlimit || wvrtvalue > wvrtlimit)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "floating conversion out of range");
	}
}

// Round directly to binary64 after checking its finite limit.
double wvrt_f128_to_f64(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_check_f128_range(wvrtvalue, DBL_MAX, wvrtfile, wvrtstart, wvrtend);
	return (double)wvrtvalue;
}

// Round directly to binary32 after checking its finite limit.
float wvrt_f128_to_f32(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_check_f128_range(wvrtvalue, FLT_MAX, wvrtfile, wvrtstart, wvrtend);
	return (float)wvrtvalue;
}

// Extended precision has its own finite limit even when it shares binary128's exponent range.
#ifdef WVRT_HAS_F80
// Round directly to native extended precision after checking its finite limit.
long double wvrt_f128_to_f80(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_check_f128_range(wvrtvalue, LDBL_MAX, wvrtfile, wvrtstart, wvrtend);
	return (long double)wvrtvalue;
}
#endif

// Compile direct half rounding when the target also supports binary16.
#ifdef WVRT_HAS_F16
// Round directly to binary16 after checking its finite limit.
_Float16 wvrt_f128_to_f16(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_check_f128_range(wvrtvalue, 65504.0, wvrtfile, wvrtstart, wvrtend);
	return (_Float16)wvrtvalue;
}
#endif
#endif

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
wvrt_array *wvrt_array_try_new(int32_t wvrtlength, size_t wvrtelement_size,
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

// Initialize owned row slots and their recursive cleanup after a successful allocation.
static wvrt_array *wvrt_prepare_rows(wvrt_array *rows) {
	if (rows == NULL) { return NULL; }
	wvrt_array **items = rows->wvrt_items;
	// Give each slot a typed null pointer before any replacement or cleanup.
	for (int32_t index = 0; index < rows->wvrt_length; ++index) {
		items[index] = NULL;
	}
	rows->wvrt_drop_element = wvrt_drop_row;
	return rows;
}

// Allocate initialized pointer slots and register their recursive cleanup.
wvrt_array *wvrt_array_new_rows_at_span(int32_t wvrtlength, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_prepare_rows(wvrt_array_new_at_span(wvrtlength, sizeof(wvrt_array *), _Alignof(wvrt_array *),
		wvrtfile, wvrtstart, wvrtend));
}

// A missing row slot has no byte allocation to lend or transfer.
static wvrt_array *wvrt_array_live_row(wvrt_array *const *wvrtitems, size_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_array *wvrt_row = wvrtitems[wvrtindex];
	if (wvrt_row == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "uninitialized byte row"); }
	return wvrt_row;
}

// Lend an initialized row without transferring its owner.
wvrt_slice wvrt_array_borrow_row_at_span(wvrt_slice wvrtrows, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_offset = wvrt_slice_offset_at_span(wvrtrows.wvrt_length, wvrtindex, wvrtfile, wvrtstart, wvrtend);
	wvrt_array *const *wvrt_items = wvrtrows.wvrt_items;
	return wvrt_array_borrow_shared(wvrt_array_live_row(wvrt_items, wvrt_offset, wvrtfile, wvrtstart, wvrtend));
}

// Select the backing descriptor owned by a populated slot without transferring it.
wvrt_array *wvrt_array_row_owner_at_span(wvrt_array *rows, int32_t index,
	int32_t file, int32_t start, int32_t end) {
	size_t offset = wvrt_slice_offset_at_span(rows->wvrt_length, index, file, start, end);
	return wvrt_array_live_row(rows->wvrt_items, offset, file, start, end);
}

// Transfer one fresh or moved row and retire the owner that previously occupied the slot.
void wvrt_array_store_row_at_span(wvrt_mut_slice wvrtrows, int32_t wvrtindex, wvrt_array *wvrtrow,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_offset = wvrt_slice_offset_at_span(wvrtrows.wvrt_length, wvrtindex, wvrtfile, wvrtstart, wvrtend);
	wvrt_array **wvrt_items = wvrtrows.wvrt_items;
	wvrt_array_drop(wvrt_items[wvrt_offset]);
	wvrt_items[wvrt_offset] = wvrtrow;
}

// Allocate inactive record slots and register their exact recursive cleanup.
wvrt_array *wvrt_array_new_owned_at_span(int32_t length, size_t size, size_t alignment,
	void (*drop)(void *), int32_t file, int32_t start, int32_t end) {
	wvrt_array *owner = wvrt_array_new_at_span(length, size, alignment, file, start, end);
	owner->wvrt_drop_element = drop;
	return owner;
}

// Reject access to an inline owner that has already moved out of its slot.
void wvrt_inline_slot_require(bool live, int32_t file, int32_t start, int32_t end) {
	if (!live) { wvrt_trap_at_span(file, start, end, "uninitialized fixed array element"); }
}

// An unfilled slot has no record to lend or transfer.
static const void *wvrt_array_live_slot(const void *items, size_t offset, size_t stride,
	int32_t file, int32_t start, int32_t end) {
	const unsigned char *slot = (const unsigned char *)items + offset * stride;
	if (!*(const bool *)slot) { wvrt_trap_at_span(file, start, end, "uninitialized array element"); }
	return slot;
}

// Lend a populated record slot through a shared view.
const void *wvrt_array_borrow_owned_at_span(wvrt_slice view, int32_t index,
	int32_t file, int32_t start, int32_t end) {
	size_t offset = wvrt_slice_offset_at_span(view.wvrt_length, index, file, start, end);
	return wvrt_array_live_slot(view.wvrt_items, offset, view.wvrt_element_size, file, start, end);
}

// Reuse the presence check and keep the permission carried by the original mutable view.
void *wvrt_array_borrow_mut_owned_at_span(wvrt_mut_slice view, int32_t index,
	int32_t file, int32_t start, int32_t end) {
	return (void *)wvrt_array_borrow_owned_at_span(wvrt_share_mut_slice(view), index, file, start, end);
}

// Retire the previous owner before installing the captured replacement.
void wvrt_array_store_owned_at_span(wvrt_mut_slice view, int32_t index, void *slot,
	void (*drop)(void *), int32_t file, int32_t start, int32_t end) {
	if (index < 0 || index >= view.wvrt_length) { drop(slot); }
	size_t offset = wvrt_slice_offset_at_span(view.wvrt_length, index, file, start, end);
	unsigned char *destination = (unsigned char *)view.wvrt_items + offset * view.wvrt_element_size;
	drop(destination);
	memcpy(destination, slot, view.wvrt_element_size);
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

// Return the present native record address without taking ownership.
const void *wvrt_require_borrow_at_span(const void *wvrtvalue, int32_t wvrtfile,
	int32_t wvrtstart, int32_t wvrtend) {
	if (wvrtvalue == NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "missing optional borrowed record"); }
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

WVRT_DECLARE_RECORD_WORKERS()

// Copy a record into transport without touching the caller's original value on failure.
static wvrt_array *wvrt_worker_try_pack(const void *value, size_t size, size_t alignment) {
	wvrt_array *box = wvrt_array_try_new(1, size, alignment);
	if (box != NULL) { memcpy(box->wvrt_items, value, size); }
	return box;
}

// Take responsibility for the value even when allocating its one-slot transport fails.
wvrt_array *wvrt_worker_pack_at_span(void *value, size_t size, size_t alignment,
	void (*drop)(void *), int32_t file, int32_t start, int32_t end) {
	wvrt_array *box = wvrt_worker_try_pack(value, size, alignment);
	// The caller transferred ownership before invoking this helper.
	if (box == NULL) {
		if (drop != NULL) { drop(value); }
		wvrt_trap_at_span(file, start, end, "could not allocate worker record");
	}
	box->wvrt_drop_element = drop;
	return box;
}

// Move the sole live element out before releasing the transport.
void wvrt_worker_unpack(wvrt_array *box, void *value) {
	memcpy(value, box->wvrt_items, box->wvrt_element_size);
	box->wvrt_length = 0;
	wvrt_array_drop(box);
}

typedef struct {
	wvrt_array *(*array)(wvrt_array *);
	wvrt_record_worker record;
} WvrtWorkerEntry;

typedef struct {
	int32_t file;
	int32_t start;
	int32_t end;
} WvrtWorkerSite;

// A successful launch transfers the input to its handler until normal Wove return.
#ifdef WVRT_POSIX_WORKERS
struct wvrt_worker_receipt {
	WvrtWorkerEntry handler;
	wvrt_array *job;
	wvrt_array *result;
	atomic_bool done;
	pthread_t thread;
	WvrtWorkerSite origin;
	WvrtWorkerPool *pool;
};

// Publish the last frame access on both normal return and native thread exit.
static void wvrt_worker_done(void *context) {
	wvrt_worker_receipt *worker = context;
	atomic_store_explicit(&worker->done, true, memory_order_release);
}

// Invoke either handler ABI and pass the launch site to record adapters that pack a result.
static wvrt_array *wvrt_worker_invoke(wvrt_worker_receipt *worker) {
	if (worker->handler.record == NULL) { return worker->handler.array(worker->job); }
	WvrtWorkerSite site = worker->origin;
	return worker->handler.record(worker->job, site.file, site.start, site.end);
}

// The generated handler consumes the job and returns its owned result by value.
static void *wvrt_worker_run(void *context) {
	wvrt_worker_receipt *worker = context;
	pthread_cleanup_push(wvrt_worker_done, worker);
	worker->result = wvrt_worker_invoke(worker);
	pthread_cleanup_pop(1);
	return NULL;
}

// Drop a completed result when pending native cancellation interrupts delivery.
static void wvrt_worker_cancel_delivery(void *result) {
	wvrt_array_drop(result);
}

// Exclude cancellation while joining or losing the only pointer to a live worker.
static int wvrt_worker_protect(WvrtWorkerSite site, wvrt_array *job) {
	int previous;
	// A joined launch still owns its job here. A submitted receipt has no input to drop.
	if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &previous) != 0) {
		wvrt_array_drop(job);
		wvrt_trap_at_span(site.file, site.start, site.end, "could not protect joined worker");
	}
	return previous;
}

// The producer owns admission and shutdown. A permit or receipt keeps one reference to the state.
struct WvrtWorkerPool {
	pthread_mutex_t lock;
	pthread_cond_t available;
	pthread_cond_t completed;
	pthread_cond_t admission;
	pthread_t *threads;
	wvrt_worker_receipt **queue;
	size_t started;
	size_t capacity;
	size_t first;
	size_t waiting;
	size_t reserved;
	size_t references;
	bool accepting;
	bool stopping;
};

static const wvrt_location wvrt_pool_site = {"<worker pool>", 1, 1};

// Report failed synchronization before operating on state that may no longer be locked.
static void wvrt_pool_check(int error, const char *message) {
	if (error != 0) { wvrt_trap(wvrt_pool_site, message); }
}

// Drain and retirement may join threads while their ownership transfer is still in progress.
static int wvrt_pool_disable_cancellation(void) {
	int previous;
	wvrt_pool_check(pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &previous), "could not protect worker pool cleanup");
	return previous;
}

// An abnormal native exit cannot strand a receipt or reduce the pool to zero workers.
static void wvrt_pool_abandoned(void *context) {
	wvrt_worker_receipt *frame = context;
	const WvrtWorkerSite site = frame->origin;
	wvrt_trap_at_span(site.file, site.start, site.end, "pooled worker exited without returning its result");
}

// A job may enter native code. Keep its exit handler active until cancellation is disabled again.
static void wvrt_pool_execute(wvrt_worker_receipt *frame) {
	pthread_cleanup_push(wvrt_pool_abandoned, frame);
	frame->result = wvrt_worker_invoke(frame);
	(void)wvrt_pool_disable_cancellation();
	pthread_cleanup_pop(0);
}

// Dispatch accepted frames until the producer has stopped the queue and every job has started.
static void *wvrt_pool_thread(void *context) {
	WvrtWorkerPool *pool = context;
	(void)wvrt_pool_disable_cancellation();
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not lock worker queue");
	// The queue and the receipt publication predicate use the same mutex.
	for (;;) {
		// A closing pool can exit only after all queued jobs have been taken.
		while (pool->waiting == 0 && !pool->stopping) {
			wvrt_pool_check(pthread_cond_wait(&pool->available, &pool->lock), "could not wait for pool work");
		}
		if (pool->waiting == 0) { break; }
		wvrt_worker_receipt *frame = pool->queue[pool->first];
		pool->first = (pool->first + 1) % pool->capacity;
		--pool->waiting;
		wvrt_pool_check(pthread_cond_signal(&pool->admission), "could not signal pool capacity");
		wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock worker queue");
		wvrt_pool_execute(frame);
		wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not publish pooled result");
		wvrt_worker_done(frame);
		// Pool-local waiters can be interested in different frames, so each checks its own predicate.
		wvrt_pool_check(pthread_cond_broadcast(&pool->completed), "could not signal pooled result");
	}
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock stopped worker queue");
	return NULL;
}

// Stop the native workers only once admission has closed and no permit can add another job.
static bool wvrt_pool_stop_locked(WvrtWorkerPool *pool) {
	if (pool->accepting || pool->reserved != 0 || pool->stopping) { return false; }
	pool->stopping = true;
	wvrt_pool_check(pthread_cond_broadcast(&pool->available), "could not wake workers for shutdown");
	return true;
}

// Join every initialized thread without holding the queue lock.
static void wvrt_pool_join(WvrtWorkerPool *pool) {
	// Startup rollback uses the same initialized prefix as graceful shutdown.
	while (pool->started != 0) {
		--pool->started;
		wvrt_pool_check(pthread_join(pool->threads[pool->started], NULL), "could not join pooled worker");
	}
	free(pool->threads);
	pool->threads = NULL;
	free(pool->queue);
	pool->queue = NULL;
}

// A completed receipt can outlive the threads and keeps its completion synchronization live.
static void wvrt_pool_release(WvrtWorkerPool *pool) {
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not lock pool ownership");
	bool last = --pool->references == 0;
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock pool ownership");
	// The final holder releases the state only after joined shutdown.
	if (last) {
		wvrt_pool_check(pthread_cond_destroy(&pool->admission), "could not release pool admission");
		wvrt_pool_check(pthread_cond_destroy(&pool->completed), "could not release pool completion");
		wvrt_pool_check(pthread_cond_destroy(&pool->available), "could not release pool work signal");
		wvrt_pool_check(pthread_mutex_destroy(&pool->lock), "could not release pool mutex");
		free(pool);
	}
}

// Allocate the waiting queue and roll back all resources if any part of startup fails.
WvrtWorkerPool *wvrt_worker_pool_open(int32_t threads, int32_t capacity) {
	if (threads <= 0 || capacity <= 0) { return NULL; }
	WvrtWorkerPool *pool = calloc(1, sizeof *pool);
	if (pool == NULL) { return NULL; }
	pool->threads = calloc((size_t)threads, sizeof *pool->threads);
	pool->queue = calloc((size_t)capacity, sizeof *pool->queue);
	if (pool->threads == NULL || pool->queue == NULL) { goto wvrt_storage; }
	if (pthread_mutex_init(&pool->lock, NULL) != 0) { goto wvrt_storage; }
	if (pthread_cond_init(&pool->available, NULL) != 0) { goto wvrt_unlock; }
	if (pthread_cond_init(&pool->completed, NULL) != 0) { goto wvrt_available; }
	if (pthread_cond_init(&pool->admission, NULL) != 0) { goto wvrt_completed; }
	pool->capacity = (size_t)capacity;
	pool->accepting = true;
	pool->references = 1;
	// Workers see complete shared state before the first one is started.
	while (pool->started < (size_t)threads) {
		// A failed start still leaves a valid owner for closing the initialized prefix.
		if (pthread_create(&pool->threads[pool->started], NULL, wvrt_pool_thread, pool) != 0) {
			wvrt_worker_pool_drop(pool);
			return NULL;
		}
		++pool->started;
	}
	return pool;
wvrt_completed:
	(void)pthread_cond_destroy(&pool->completed);
wvrt_available:
	(void)pthread_cond_destroy(&pool->available);
wvrt_unlock:
	(void)pthread_mutex_destroy(&pool->lock);
wvrt_storage:
	free(pool->queue);
	free(pool->threads);
	free(pool);
	return NULL;
}

// The locked queue counts waiting frames and unused permits against the same capacity.
static bool wvrt_pool_can_reserve(const WvrtWorkerPool *pool) {
	return pool->accepting &&
		pool->reserved < pool->capacity - pool->waiting &&
		pool->references != SIZE_MAX;
}

// Claim a waiting slot and give the new permit its own reference to the pool.
static WvrtWorkerPermit *wvrt_pool_reserve_locked(WvrtWorkerPool *pool) {
	if (!wvrt_pool_can_reserve(pool)) { return NULL; }
	++pool->reserved;
	++pool->references;
	return pool;
}

// Reserve a waiting slot without taking any job data from the producer.
WvrtWorkerPermit *wvrt_worker_pool_reserve(WvrtWorkerPool *pool) {
	if (pool == NULL) { return NULL; }
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not lock pool admission");
	WvrtWorkerPermit *permit = wvrt_pool_reserve_locked(pool);
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock pool admission");
	return permit;
}

// Condition waits reacquire the mutex before cancellation runs their cleanup.
static void wvrt_pool_unlock_admission(void *context) {
	WvrtWorkerPool *pool = context;
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock waiting pool admission");
}

// Wait only while a queued frame can free capacity independently of the producer.
WvrtWorkerPermit *wvrt_worker_pool_reserve_wait(WvrtWorkerPool *pool) {
	if (pool == NULL) { return NULL; }
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not lock waiting pool admission");
	WvrtWorkerPermit *permit;
	pthread_cleanup_push(wvrt_pool_unlock_admission, pool);
	// Full capacity held solely by unused permits requires the producer to act.
	while (pool->accepting && pool->waiting != 0 &&
		pool->references != SIZE_MAX && !wvrt_pool_can_reserve(pool)) {
		wvrt_pool_check(pthread_cond_wait(&pool->admission, &pool->lock), "could not wait for pool capacity");
	}
	permit = wvrt_pool_reserve_locked(pool);
	pthread_cleanup_pop(1);
	return permit;
}

// An explicit close leaves outstanding reservations usable and closes once they have all been resolved.
bool wvrt_worker_pool_close(WvrtWorkerPool *pool) {
	if (pool == NULL) { return false; }
	int previous = wvrt_pool_disable_cancellation();
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not lock worker pool shutdown");
	bool closed = pool->reserved == 0;
	// Wake admission waiters before draining work from a closed queue.
	if (closed) {
		pool->accepting = false;
		wvrt_pool_check(pthread_cond_broadcast(&pool->admission), "could not signal closed admission");
	}
	bool join = wvrt_pool_stop_locked(pool);
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock worker pool shutdown");
	if (join) { wvrt_pool_join(pool); }
	(void)pthread_setcancelstate(previous, NULL);
	return closed;
}

// Retire the public owner. Unused reservations keep the already admitted capacity live.
void wvrt_worker_pool_drop(WvrtWorkerPool *pool) {
	if (pool == NULL) { return; }
	int previous = wvrt_pool_disable_cancellation();
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not retire worker pool owner");
	pool->accepting = false;
	wvrt_pool_check(pthread_cond_broadcast(&pool->admission), "could not signal retired admission");
	bool join = wvrt_pool_stop_locked(pool);
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock retired worker pool");
	if (join) { wvrt_pool_join(pool); }
	wvrt_pool_release(pool);
	(void)pthread_setcancelstate(previous, NULL);
}

// Cancel unused admission and finish a deferred shutdown if this was its last reservation.
void wvrt_worker_permit_drop(WvrtWorkerPermit *pool) {
	if (pool == NULL) { return; }
	int previous = wvrt_pool_disable_cancellation();
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not release worker reservation");
	--pool->reserved;
	wvrt_pool_check(pthread_cond_signal(&pool->admission), "could not signal released admission");
	bool join = wvrt_pool_stop_locked(pool);
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock released worker reservation");
	if (join) { wvrt_pool_join(pool); }
	wvrt_pool_release(pool);
	(void)pthread_setcancelstate(previous, NULL);
}

// Convert one reservation into a queued frame and transfer its reference to the receipt.
static void wvrt_pool_enqueue(WvrtWorkerPermit *pool, wvrt_worker_receipt *frame) {
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not enqueue pooled worker");
	frame->pool = pool;
	--pool->reserved;
	size_t next = (pool->first + pool->waiting) % pool->capacity;
	pool->queue[next] = frame;
	++pool->waiting;
	wvrt_pool_check(pthread_cond_signal(&pool->available), "could not signal pooled job");
	bool join = wvrt_pool_stop_locked(pool);
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock queued worker");
	if (join) { wvrt_pool_join(pool); }
}

// Retire a pooled receipt without joining its reusable worker thread.
static const char *wvrt_pool_retire(wvrt_worker_receipt *worker, wvrt_array **result) {
	WvrtWorkerPool *pool = worker->pool;
	wvrt_pool_check(pthread_mutex_lock(&pool->lock), "could not lock pooled result");
	// Publication and retirement of the last frame access share the pool mutex.
	while (!atomic_load_explicit(&worker->done, memory_order_acquire)) {
		wvrt_pool_check(pthread_cond_wait(&pool->completed, &pool->lock), "could not wait for pooled result");
	}
	*result = worker->result;
	wvrt_pool_check(pthread_mutex_unlock(&pool->lock), "could not unlock pooled result");
	free(worker);
	wvrt_pool_release(pool);
	return *result == NULL ? "joined worker exited without returning" : NULL;
}

// Initialize the frame before passing it to a thread or scheduler.
static void wvrt_worker_prepare(wvrt_worker_receipt *worker, WvrtWorkerEntry handler,
	wvrt_array *job, WvrtWorkerSite site) {
	worker->handler = handler;
	worker->job = job;
	worker->result = NULL;
	worker->origin = site;
	worker->pool = NULL;
	atomic_init(&worker->done, false);
}

// Only a successfully created thread owns the input in the prepared frame.
static bool wvrt_worker_try_start(wvrt_worker_receipt *worker, WvrtWorkerEntry handler,
	wvrt_array *job, WvrtWorkerSite site) {
	wvrt_worker_prepare(worker, handler, job, site);
	return pthread_create(&worker->thread, NULL, wvrt_worker_run, worker) == 0;
}

// Start a frame with either lexical storage in the joined call or heap storage in a receipt.
static void wvrt_worker_start(wvrt_worker_receipt *worker, WvrtWorkerEntry handler,
	wvrt_array *job, WvrtWorkerSite site, bool submitted) {
	// An unsuccessful launch leaves both the job and the frame with the submitting thread.
	if (!wvrt_worker_try_start(worker, handler, job, site)) {
		if (submitted) { free(worker); }
		wvrt_array_drop(job);
		wvrt_trap_at_span(site.file, site.start, site.end,
			submitted ? "could not start submitted worker" : "could not start joined worker");
	}
}

// Wait for the frame's final access, then let the caller handle its result and any error.
static const char *wvrt_worker_retire(wvrt_worker_receipt *worker, bool submitted, wvrt_array **result) {
	*result = NULL;
	if (worker == NULL) { return "cannot join an empty worker receipt"; }
	if (worker->pool != NULL) { return wvrt_pool_retire(worker, result); }
	const bool joined = pthread_join(worker->thread, NULL) == 0;
	// Wait for publication if a failed join left the child running.
	if (!joined) {
		// The worker owns the frame until its final access has been published.
		while (!atomic_load_explicit(&worker->done, memory_order_acquire)) {
			(void)sched_yield();
		}
	}
	*result = worker->result;
	if (submitted) { free(worker); }
	if (!joined) { return "could not join worker"; }
	if (*result == NULL) { return "joined worker exited without returning"; }
	return NULL;
}

// Transfer the completed owner while releasing it if pending cancellation interrupts delivery.
static wvrt_array *wvrt_worker_deliver(wvrt_array *result, int previous_cancel) {
	pthread_cleanup_push(wvrt_worker_cancel_delivery, result);
	(void)pthread_setcancelstate(previous_cancel, NULL);
	pthread_testcancel();
	pthread_cleanup_pop(0);
	return result;
}

// Retire a single worker, delivering its result or dropping it at scope exit.
static wvrt_array *wvrt_worker_finish(wvrt_worker_receipt *worker, WvrtWorkerSite site,
	int previous_cancel, bool submitted, bool deliver) {
	wvrt_array *result;
	const char *error = wvrt_worker_retire(worker, submitted, &result);
	// A failed join still releases the array the handler produced.
	if (error != NULL) {
		wvrt_array_drop(result);
		wvrt_trap_at_span(site.file, site.start, site.end, error);
	}
	// Scope cleanup takes responsibility for an unclaimed result.
	if (!deliver) {
		wvrt_array_drop(result);
		result = NULL;
	}
	return wvrt_worker_deliver(result, previous_cancel);
}
#else
// Targets without native threads report an unavailable pool through the open result.
WvrtWorkerPool *wvrt_worker_pool_open(int32_t threads, int32_t capacity) {
	(void)threads;
	(void)capacity;
	return NULL;
}

// There is no waiting capacity on targets without native threads.
WvrtWorkerPermit *wvrt_worker_pool_reserve(WvrtWorkerPool *pool) {
	(void)pool;
	return NULL;
}

// Waiting cannot make reservation possible without a native thread pool.
WvrtWorkerPermit *wvrt_worker_pool_reserve_wait(WvrtWorkerPool *pool) {
	return wvrt_worker_pool_reserve(pool);
}

// A missing pool cannot be explicitly closed.
bool wvrt_worker_pool_close(WvrtWorkerPool *pool) {
	(void)pool;
	return false;
}

// A lexical owner may contain the null result of open.
void wvrt_worker_pool_drop(WvrtWorkerPool *pool) {
	(void)pool;
}

// Null reservations need no cleanup.
void wvrt_worker_permit_drop(WvrtWorkerPermit *permit) {
	(void)permit;
}
#endif

// Transfer the lexical pool or permit owner out of its original slot.
WvrtWorkerPool *wvrt_worker_pool_take(WvrtWorkerPool **slot) {
	WvrtWorkerPool *pool = *slot;
	*slot = NULL;
	return pool;
}

// Reject failed reservation before the generated submission evaluates its job operand.
WvrtWorkerPermit *wvrt_worker_permit_take_at_span(WvrtWorkerPermit **slot,
	int32_t file, int32_t start, int32_t end) {
	if (*slot == NULL) { wvrt_trap_at_span(file, start, end, "cannot submit without a worker permit"); }
	return wvrt_worker_pool_take(slot);
}

#ifdef WVRT_POSIX_WORKERS
// Retire an enqueued receipt if cancellation interrupts its delivery to the producer.
static void wvrt_pool_cancel_receipt(void *receipt) {
	wvrt_worker_receipt_drop(receipt);
}
#endif

// Consume the reservation and the already evaluated job, returning an ordinary worker receipt.
static wvrt_worker_receipt *wvrt_worker_submit_on(WvrtWorkerEntry handler,
	WvrtWorkerPermit *permit, wvrt_array *job, WvrtWorkerSite site) {
	// A native caller can bypass the generated reservation check.
	if (permit == NULL) {
		wvrt_array_drop(job);
		wvrt_trap_at_span(site.file, site.start, site.end, "cannot submit without a worker permit");
	}
#ifdef WVRT_POSIX_WORKERS
	const int previous = wvrt_pool_disable_cancellation();
	wvrt_worker_receipt *worker = malloc(sizeof *worker);
	// The failed submission still owns both operands and must release them.
	if (worker == NULL) {
		wvrt_array_drop(job);
		wvrt_worker_permit_drop(permit);
		wvrt_trap_at_span(site.file, site.start, site.end, "could not allocate pooled worker");
	}
	wvrt_worker_prepare(worker, handler, job, site);
	wvrt_pool_enqueue(permit, worker);
	pthread_cleanup_push(wvrt_pool_cancel_receipt, worker);
	(void)pthread_setcancelstate(previous, NULL);
	pthread_testcancel();
	pthread_cleanup_pop(0);
	return worker;
#else
	(void)handler;
	wvrt_array_drop(job);
	wvrt_trap_at_span(site.file, site.start, site.end, "could not start pooled worker");
#endif
}

// Submit the original array-to-array handler and its existing descriptor.
wvrt_worker_receipt *wvrt_worker_spawn_on_at_span(wvrt_array *(*handler)(wvrt_array *),
	WvrtWorkerPermit *permit, wvrt_array *job, int32_t file, int32_t start, int32_t end) {
	return wvrt_worker_submit_on((WvrtWorkerEntry){.array = handler}, permit, job, (WvrtWorkerSite){file, start, end});
}

// Box an inline job before admission. A zero size passes an existing array descriptor.
wvrt_worker_receipt *wvrt_worker_spawn_on_record_at_span(wvrt_record_worker handler, WvrtWorkerPermit *permit,
	void *job, size_t size, size_t alignment, void (*drop)(void *), int32_t file, int32_t start, int32_t end) {
	// A native caller can bypass permit validation before evaluating the job.
	if (size != 0 && permit == NULL) {
		if (drop != NULL) { drop(job); }
		wvrt_trap_at_span(file, start, end, "cannot submit without a worker permit");
	}
	// Pack only inline values. An array job can pass through its existing descriptor.
	if (size != 0) {
		wvrt_array *box = wvrt_worker_try_pack(job, size, alignment);
		// Until the box exists, this submission still owns the original inline value.
		if (box == NULL) {
			if (drop != NULL) { drop(job); }
			wvrt_worker_permit_drop(permit);
			wvrt_trap_at_span(file, start, end, "could not allocate worker record");
		}
		box->wvrt_drop_element = drop;
		job = box;
	}
	return wvrt_worker_submit_on((WvrtWorkerEntry){.record = handler}, permit, job, (WvrtWorkerSite){file, start, end});
}

// Keep the joined call's frame on the stack until the thread has stopped using it.
static wvrt_array *wvrt_worker_run_joined(WvrtWorkerEntry handler, wvrt_array *job, WvrtWorkerSite site) {
#ifdef WVRT_POSIX_WORKERS
	const int previous = wvrt_worker_protect(site, job);
	wvrt_worker_receipt worker;
	wvrt_worker_start(&worker, handler, job, site, false);
	return wvrt_worker_finish(&worker, site, previous, false, true);
#else
	(void)handler;
	wvrt_array_drop(job);
	wvrt_trap_at_span(site.file, site.start, site.end, "could not start joined worker");
#endif
}

// Transfer a job to a heap-backed frame whose lifetime extends beyond submission.
static wvrt_worker_receipt *wvrt_worker_submit(WvrtWorkerEntry handler, wvrt_array *job, WvrtWorkerSite site) {
#ifdef WVRT_POSIX_WORKERS
	wvrt_worker_receipt *worker = malloc(sizeof *worker);
	// Allocation failure leaves the evaluated input with the submitting scope.
	if (worker == NULL) {
		wvrt_array_drop(job);
		wvrt_trap_at_span(site.file, site.start, site.end, "could not allocate submitted worker");
	}
	wvrt_worker_start(worker, handler, job, site, true);
	return worker;
#else
	(void)handler;
	wvrt_array_drop(job);
	wvrt_trap_at_span(site.file, site.start, site.end, "could not start submitted worker");
#endif
}

// Launch and collect the original array-to-array handler without boxing.
wvrt_array *wvrt_worker_spawn_join_at_span(wvrt_array *(*handler)(wvrt_array *), wvrt_array *job,
	int32_t file, int32_t start, int32_t end) {
	return wvrt_worker_run_joined((WvrtWorkerEntry){.array = handler}, job, (WvrtWorkerSite){file, start, end});
}

// Submit the original array-to-array handler without boxing.
wvrt_worker_receipt *wvrt_worker_spawn_at_span(wvrt_array *(*handler)(wvrt_array *), wvrt_array *job,
	int32_t file, int32_t start, int32_t end) {
	return wvrt_worker_submit((WvrtWorkerEntry){.array = handler}, job, (WvrtWorkerSite){file, start, end});
}

// Launch and collect an ordinary Wove handler with a record at either endpoint.
wvrt_array *wvrt_worker_spawn_join_record_at_span(wvrt_record_worker handler, wvrt_array *job,
	int32_t file, int32_t start, int32_t end) {
	return wvrt_worker_run_joined((WvrtWorkerEntry){.record = handler}, job, (WvrtWorkerSite){file, start, end});
}

// Submit the adapted handler to a fresh thread and keep the standard lexical receipt.
wvrt_worker_receipt *wvrt_worker_spawn_record_at_span(wvrt_record_worker handler, wvrt_array *job,
	int32_t file, int32_t start, int32_t end) {
	return wvrt_worker_submit((WvrtWorkerEntry){.record = handler}, job, (WvrtWorkerSite){file, start, end});
}

// Clear the original lexical slot before handing its frame to a new owner.
wvrt_worker_receipt *wvrt_worker_receipt_take(wvrt_worker_receipt **slot) {
	wvrt_worker_receipt *worker = *slot;
	*slot = NULL;
	return worker;
}

// Consume the receipt and deliver its result at the explicit join site.
wvrt_array *wvrt_worker_join_at_span(wvrt_worker_receipt **slot, int32_t file, int32_t start, int32_t end) {
#ifdef WVRT_POSIX_WORKERS
	const WvrtWorkerSite site = {file, start, end};
	// A receipt with an empty slot has already transferred its frame.
	if (*slot != NULL) {
		const int previous = wvrt_worker_protect(site, NULL);
		return wvrt_worker_finish(wvrt_worker_receipt_take(slot), site, previous, true, true);
	}
#else
	(void)slot;
#endif
	wvrt_trap_at_span(file, start, end, "cannot join an empty worker receipt");
}

// An untouched lexical receipt waits and drops its result at the original submission site.
void wvrt_worker_receipt_drop(wvrt_worker_receipt *worker) {
#ifdef WVRT_POSIX_WORKERS
	if (worker == NULL) { return; }
	const WvrtWorkerSite site = worker->origin;
	const int previous = wvrt_worker_protect(site, NULL);
	(void)wvrt_worker_finish(worker, site, previous, true, false);
#else
	(void)worker;
#endif
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

// Preserve the current allocation if it already covers the requested live extent.
static bool wvrt_array_try_grow_to(wvrt_array *buffer, int32_t length) {
	if (length <= buffer->wvrt_capacity) { return true; }
	int32_t capacity = buffer->wvrt_capacity;
	int32_t next = capacity > INT32_MAX / 2 ? INT32_MAX : capacity * 2;
	if (next < length) { next = length; }
	return wvrt_array_try_reserve(buffer, next);
}

// Copy the complete checked view only when there is room for all of it.
bool wvrt_array_try_extend(wvrt_array *buffer, wvrt_slice source) {
	if (source.wvrt_length == 0) { return true; }
	int32_t length = buffer->wvrt_length;
	size_t size = buffer->wvrt_element_size;
	// Reject an unrepresentable length or a descriptor with a different element stride.
	if (source.wvrt_length < 0 || source.wvrt_length > INT32_MAX - length ||
		source.wvrt_element_size != size) {
		return false;
	}
	int32_t next = length + source.wvrt_length;
	if (!wvrt_array_try_grow_to(buffer, next)) { return false; }
	memcpy((unsigned char *)buffer->wvrt_items + (size_t)length * size,
		source.wvrt_items, (size_t)source.wvrt_length * size);
	buffer->wvrt_length = next;
	buffer->wvrt_spare = 0;
	return true;
}

// Copy the entire source prefix after validating both descriptors and their separation.
bool wvrt_slice_try_copy(wvrt_mut_slice destination, wvrt_slice source) {
	size_t size = destination.wvrt_element_size;
	// A failed shape or element-stride check leaves both views untouched.
	if (source.wvrt_length < 0 || source.wvrt_length > destination.wvrt_length ||
		size == 0 || source.wvrt_element_size != size ||
		(size_t)destination.wvrt_length > SIZE_MAX / size) {
		return false;
	}
	if (source.wvrt_length == 0) { return true; }
	if (destination.wvrt_items == NULL || source.wvrt_items == NULL) { return false; }
	size_t source_bytes = (size_t)source.wvrt_length * size;
	size_t destination_bytes = (size_t)destination.wvrt_length * size;
	uintptr_t target = (uintptr_t)destination.wvrt_items;
	uintptr_t input = (uintptr_t)source.wvrt_items;
	// Checked Wove loans are disjoint. Refuse overlapping descriptors at the C boundary as well.
	if (target <= input ? input - target < destination_bytes : target - input < source_bytes) {
		return false;
	}
	memcpy(destination.wvrt_items, source.wvrt_items, source_bytes);
	return true;
}

// Append a captured value, leaving the live prefix intact if storage cannot grow.
static const char *wvrt_array_try_append(wvrt_array *buffer, const void *value) {
	int32_t length = buffer->wvrt_length;
	if (length == INT32_MAX) { return "array length overflow"; }
	if (!wvrt_array_try_grow_to(buffer, length + 1)) { return "could not grow array"; }
	size_t size = buffer->wvrt_element_size;
	memcpy((unsigned char *)buffer->wvrt_items + (size_t)length * size, value, size);
	buffer->wvrt_length = length + 1;
	buffer->wvrt_spare = 0;
	return NULL;
}

// The caller keeps its captured value alive until a successful append.
bool wvrt_array_try_push(wvrt_array *buffer, const void *value) {
	return wvrt_array_try_append(buffer, value) == NULL;
}

// Append a value captured outside the array, growing geometrically when needed.
void wvrt_array_push_at_span(wvrt_array *wvrtbuffer, const void *wvrtvalue,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	const char *error = wvrt_array_try_append(wvrtbuffer, wvrtvalue);
	if (error != NULL) { wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, error); }
}

// Reserve receipt space before making any transfer visible to a new thread.
// A nonzero size requests a record box; size zero passes the original array descriptor.
bool wvrt_worker_try_spawn_at_span(wvrt_array *queue, wvrt_array *(*array)(wvrt_array *),
	wvrt_record_worker record, void *job, size_t size, size_t alignment, int32_t file, int32_t start, int32_t end) {
#ifdef WVRT_POSIX_WORKERS
	if (queue == NULL || job == NULL) { return false; }
	int32_t length = queue->wvrt_length;
	if (length == INT32_MAX || !wvrt_array_try_grow_to(queue, length + 1)) { return false; }
	wvrt_array *payload = size == 0 ? job : wvrt_worker_try_pack(job, size, alignment);
	if (payload == NULL) { return false; }
	wvrt_worker_receipt *worker = malloc(sizeof *worker);
	WvrtWorkerEntry handler = {.array = array, .record = record};
	WvrtWorkerSite site = {file, start, end};
	// Until launch succeeds the caller still owns the job. Discard only the transport.
	if (worker == NULL || !wvrt_worker_try_start(worker, handler, payload, site)) {
		free(worker);
		if (size != 0) { wvrt_array_drop(payload); }
		return false;
	}
	// The exclusive queue already has room. Publication cannot allocate or fail.
	((wvrt_worker_receipt **)queue->wvrt_items)[length] = worker;
	queue->wvrt_length = length + 1;
	queue->wvrt_spare = 0;
	return true;
#else
	(void)queue;
	(void)array;
	(void)record;
	(void)job;
	(void)size;
	(void)alignment;
	(void)file;
	(void)start;
	(void)end;
	return false;
#endif
}

// Create the empty owner of a FIFO sequence of joinable worker frames.
wvrt_array *wvrt_worker_queue_at_span(int32_t file, int32_t start, int32_t end) {
	wvrt_array *queue = wvrt_array_try_new(0, sizeof(wvrt_worker_receipt *), _Alignof(wvrt_worker_receipt *));
	if (queue == NULL) { wvrt_trap_at_span(file, start, end, "could not allocate worker receipt queue"); }
	return queue;
}

#ifdef WVRT_POSIX_WORKERS
// Retire every remaining receipt, remembering the first failed worker's submission span.
static const char *wvrt_worker_queue_drain(wvrt_array *queue, WvrtWorkerSite *site) {
	const char *first_error = NULL;
	if (queue == NULL) { return NULL; }
	wvrt_worker_receipt **items = queue->wvrt_items;
	// Collection clears a slot before retiring it, so a partial drain skips completed frames.
	for (int32_t index = 0; index < queue->wvrt_length; ++index) {
		wvrt_worker_receipt *worker = items[index];
		if (worker == NULL) { continue; }
		WvrtWorkerSite origin = worker->origin;
		wvrt_array *result;
		const char *error = wvrt_worker_retire(worker, true, &result);
		wvrt_array_drop(result);
		// A later join error cannot prevent other frames from being retired.
		if (error != NULL && first_error == NULL) {
			first_error = error;
			*site = origin;
		}
	}
	wvrt_array_drop(queue);
	return first_error;
}
#endif

// Release a lexical queue and discard every result that was never claimed.
void wvrt_worker_queue_drop(wvrt_array *queue) {
	if (queue == NULL) { return; }
#ifdef WVRT_POSIX_WORKERS
	wvrt_worker_receipt **items = queue->wvrt_items;
	WvrtWorkerSite site = queue->wvrt_length == 0 ? (WvrtWorkerSite){0, 0, 0} : items[0]->origin;
	const int previous = wvrt_worker_protect(site, NULL);
	const char *error = wvrt_worker_queue_drain(queue, &site);
	if (error != NULL) { wvrt_trap_at_span(site.file, site.start, site.end, error); }
	(void)wvrt_worker_deliver(NULL, previous);
#else
	wvrt_array_drop(queue);
#endif
}

// Append an evaluated receipt and preserve queue ownership until storage growth succeeds.
void wvrt_worker_queue_push_at_span(wvrt_array **slot, wvrt_worker_receipt *receipt,
	int32_t file, int32_t start, int32_t end) {
	if (*slot != NULL && receipt != NULL && wvrt_array_try_append(*slot, &receipt) == NULL) { return; }
#ifdef WVRT_POSIX_WORKERS
	WvrtWorkerSite site = {file, start, end};
	(void)wvrt_worker_protect(site, NULL);
	(void)wvrt_worker_queue_drain(wvrt_array_take(slot), &site);
	// The incoming receipt was transferred out of its lexical slot before the append.
	if (receipt != NULL) {
		wvrt_array *result;
		(void)wvrt_worker_retire(receipt, true, &result);
		wvrt_array_drop(result);
	}
#else
	wvrt_array_drop(wvrt_array_take(slot));
#endif
	wvrt_trap_at_span(file, start, end, "could not append worker receipt");
}

// Transfer the oldest receipt and retain the remaining receipts in enqueue order.
wvrt_worker_receipt *wvrt_worker_queue_pop_front_at_span(wvrt_array *queue,
	int32_t file, int32_t start, int32_t end) {
	wvrt_worker_receipt *receipt;
	wvrt_array_remove_at_span(queue, 0, &receipt, file, start, end);
	return receipt;
}

typedef struct {
	size_t stride;
	size_t alignment;
	size_t value_offset;
	void (*drop)(void *);
	bool boxed;
} WvrtWorkerResultLayout;

// Retire each receipt into the exact output array representation in enqueue order.
static wvrt_array *wvrt_worker_collect(wvrt_array **slot, WvrtWorkerResultLayout layout, WvrtWorkerSite site) {
	if (*slot == NULL) { wvrt_trap_at_span(site.file, site.start, site.end, "cannot join an empty worker queue slot"); }
#ifdef WVRT_POSIX_WORKERS
	const int previous = wvrt_worker_protect(site, NULL);
	wvrt_array *queue = wvrt_array_take(slot);
	wvrt_array *results = wvrt_array_try_new(queue->wvrt_length, layout.stride, layout.alignment);
	// Output allocation failure still retires all the consumed receipts.
	if (results == NULL) {
		(void)wvrt_worker_queue_drain(queue, &site);
		wvrt_trap_at_span(site.file, site.start, site.end, "could not allocate worker results");
	}
	results->wvrt_length = 0;
	results->wvrt_drop_element = layout.drop;
	wvrt_worker_receipt **pending = queue->wvrt_items;
	// Only the initialized prefix belongs to the output while collection is in flight.
	for (int32_t index = 0; index < queue->wvrt_length; ++index) {
		wvrt_worker_receipt *worker = pending[index];
		pending[index] = NULL;
		wvrt_array *result;
		const char *error = wvrt_worker_retire(worker, true, &result);
		// Release the collected prefix, a failed join's result, and all remaining receipts.
		if (error != NULL) {
			wvrt_array_drop(results);
			wvrt_array_drop(result);
			(void)wvrt_worker_queue_drain(queue, &site);
			wvrt_trap_at_span(site.file, site.start, site.end, error);
		}
		unsigned char *item = (unsigned char *)results->wvrt_items + (size_t)index * layout.stride;
		// Unpack a record into its inline value. A wrapped slot starts with its live flag.
		if (layout.boxed) {
			wvrt_worker_unpack(result, item + layout.value_offset);
			if (layout.drop != NULL) { *(bool *)item = true; }
		} else {
			*(wvrt_array **)item = result;
		}
		++results->wvrt_length;
	}
	wvrt_array_drop(queue);
	return wvrt_worker_deliver(results, previous);
#else
	wvrt_array_drop(wvrt_array_take(slot));
	wvrt_array *results = wvrt_array_new_at_span(0, layout.stride, layout.alignment, site.file, site.start, site.end);
	results->wvrt_drop_element = layout.drop;
	return results;
#endif
}

// Preserve the original nested-array ABI and transfer each result descriptor directly.
wvrt_array *wvrt_worker_join_all_at_span(wvrt_array **slot, int32_t file, int32_t start, int32_t end) {
	WvrtWorkerResultLayout layout = {sizeof(wvrt_array *), _Alignof(wvrt_array *), 0, wvrt_drop_row, false};
	return wvrt_worker_collect(slot, layout, (WvrtWorkerSite){file, start, end});
}

// Collect uniform record results into the exact generated value or owning-slot layout.
wvrt_array *wvrt_worker_join_all_records_at_span(wvrt_array **slot, size_t stride, size_t alignment,
	size_t value_offset, void (*drop)(void *), int32_t file, int32_t start, int32_t end) {
	WvrtWorkerResultLayout layout = {stride, alignment, value_offset, drop, true};
	return wvrt_worker_collect(slot, layout, (WvrtWorkerSite){file, start, end});
}

// Retire the incoming record if its slot could not be appended to the array.
void wvrt_array_push_owned_at_span(wvrt_array *owner, void *slot, int32_t file, int32_t start, int32_t end) {
	const char *error = wvrt_array_try_append(owner, slot);
	// Failure leaves the source slot as the only owner of the detached value.
	if (error != NULL) {
		owner->wvrt_drop_element(slot);
		wvrt_trap_at_span(file, start, end, error);
	}
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

// Transfer an initialized record and shift the remaining slot ownership with the tail.
void wvrt_array_remove_owned_at_span(wvrt_array *owner, int32_t index, void *slot,
	int32_t file, int32_t start, int32_t end) {
	size_t offset = wvrt_array_index_at_span(owner->wvrt_length, index, file, start, end);
	const void *source = wvrt_array_live_slot(owner->wvrt_items, offset, owner->wvrt_element_size, file, start, end);
	memcpy(slot, source, owner->wvrt_element_size);
	wvrt_array_erase(owner, offset);
}

// Transfer a populated row to its new owner. An unfilled slot has no owner to return.
wvrt_array *wvrt_array_remove_row_at_span(wvrt_array *wvrtbuffer, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_index = wvrt_array_index_at_span(wvrtbuffer->wvrt_length, wvrtindex,
		wvrtfile, wvrtstart, wvrtend);
	wvrt_array *wvrt_row = wvrt_array_live_row(wvrtbuffer->wvrt_items, wvrt_index, wvrtfile, wvrtstart, wvrtend);
	wvrt_array_erase(wvrtbuffer, wvrt_index);
	return wvrt_row;
}

// Transfer a visited row without shifting later positions or changing the iteration bound.
wvrt_array *wvrt_array_take_row_at_span(wvrt_array *wvrtbuffer, int32_t wvrtindex,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	size_t wvrt_index = wvrt_array_index_at_span(wvrtbuffer->wvrt_length, wvrtindex,
		wvrtfile, wvrtstart, wvrtend);
	wvrt_array **wvrt_items = wvrtbuffer->wvrt_items;
	wvrt_array *wvrt_row = wvrt_array_live_row(wvrt_items, wvrt_index, wvrtfile, wvrtstart, wvrtend);
	wvrt_items[wvrt_index] = NULL;
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

// Narrow a shared view only after proving the signed offset and extent fit.
wvrt_slice wvrt_slice_try_exact(wvrt_slice view, int32_t offset, int32_t extent) {
	// Subtract first so a large offset or extent cannot overflow or form an invalid pointer.
	if (offset < 0 || offset > view.wvrt_length || extent < 0 || extent > view.wvrt_length - offset) {
		return (wvrt_slice){NULL, -1, view.wvrt_element_size};
	}
	return wvrt_slice_subrange(view, offset, offset + extent);
}

// Extract a present shared view, including an empty one, at the written unwrap site.
wvrt_slice wvrt_slice_require_present_at_span(wvrt_slice view, int32_t file, int32_t start, int32_t end) {
	// A negative length is reserved for absence and is never readable as an ordinary view.
	if (view.wvrt_length < 0) {
		wvrt_trap_at_span(file, start, end, "optional view is absent");
	}
	return view;
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

// Check the exact byte view before touching any element.
static const uint8_t *wvrt_integer_bytes(wvrt_slice wvrtview, int32_t wvrtwidth,
	bool wvrtvalid, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// The caller supplies the range admitted by the result type.
	if (!wvrtvalid || wvrtview.wvrt_length != wvrtwidth ||
		wvrtview.wvrt_element_size != sizeof(uint8_t) || wvrtview.wvrt_items == NULL) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer byte width mismatch");
	}
	return wvrtview.wvrt_items;
}

// Assemble an exact-width byte view using unsigned arithmetic and unaligned byte reads.
uint64_t wvrt_int_from_bytes(wvrt_slice wvrtview, int32_t wvrtwidth, bool wvrtbigendian,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	const uint8_t *wvrtbytes = wvrt_integer_bytes(wvrtview, wvrtwidth, wvrtwidth >= 1 && wvrtwidth <= 8,
		wvrtfile, wvrtstart, wvrtend);
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

#ifdef WVRT_HAS_INT128
// Recover the signed 128-bit value using only representable signed casts.
wvrt_i128 wvrt_i128_from_wrapped_bits(wvrt_u128 wvrtbits) {
	const wvrt_u128 wvrtall = ~(wvrt_u128)0;
	if (wvrtbits <= (wvrtall >> 1)) { return (wvrt_i128)wvrtbits; }
	return -1 - (wvrt_i128)(wvrtall - wvrtbits);
}

// Assemble all sixteen bytes, including the high word, after the extent check.
wvrt_u128 wvrt_u128_from_bytes(wvrt_slice wvrtview, int32_t wvrtwidth, bool wvrtbigendian,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	const uint8_t *wvrtbytes = wvrt_integer_bytes(wvrtview, wvrtwidth, wvrtwidth == 16,
		wvrtfile, wvrtstart, wvrtend);
	wvrt_u128 wvrtbits = 0;
	// Read the bytes from the most significant end of the requested encoding.
	for (int32_t wvrtindex = 0; wvrtindex < wvrtwidth; ++wvrtindex) {
		int32_t wvrtoffset = wvrtbigendian ? wvrtindex : wvrtwidth - 1 - wvrtindex;
		wvrtbits = (wvrtbits << 8) | wvrtbytes[wvrtoffset];
	}
	return wvrtbits;
}

// Interpret a checked 16-byte representation as a signed integer.
wvrt_i128 wvrt_i128_from_bytes(wvrt_slice wvrtview, int32_t wvrtwidth, bool wvrtbigendian,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_i128_from_wrapped_bits(wvrt_u128_from_bytes(wvrtview, wvrtwidth, wvrtbigendian,
		wvrtfile, wvrtstart, wvrtend));
}
#endif

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
int32_t wvrt_shift_count_at_span(int32_t wvrtcount, int32_t width,
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

// Mask to a declared width between eight and 64 bits.
static uint64_t wvrt_word_mask(int32_t width) {
	return UINT64_MAX >> (64 - width);
}

// Count leading zeros in the declared bits, guarding the builtin's zero case.
int32_t wvrt_clz(uint64_t value, int32_t width) {
	const uint64_t bits = value & wvrt_word_mask(width);
	const int32_t builtin_width = (int32_t)(sizeof(unsigned long long) * CHAR_BIT);
	return bits == 0 ? width : __builtin_clzll(bits) - (builtin_width - width);
}

// Count trailing zeros in the declared bits, including a whole zero word.
int32_t wvrt_ctz(uint64_t value, int32_t width) {
	const uint64_t bits = value & wvrt_word_mask(width);
	return bits == 0 ? width : __builtin_ctzll(bits);
}

// Count only the set bits in the declared width.
int32_t wvrt_popcnt(uint64_t value, int32_t width) {
	return __builtin_popcountll(value & wvrt_word_mask(width));
}

// The wide counters combine 64-bit halves on targets with the native ABI.
#ifdef WVRT_HAS_INT128
// Leading zeros cross into the low half only when the high half is zero.
int32_t wvrt_clz128(wvrt_u128 value) {
	const uint64_t high = (uint64_t)(value >> 64);
	return high == 0 ? 64 + wvrt_clz((uint64_t)value, 64) : wvrt_clz(high, 64);
}

// Trailing zeros cross into the high half only when the low half is zero.
int32_t wvrt_ctz128(wvrt_u128 value) {
	const uint64_t low = (uint64_t)value;
	return low == 0 ? 64 + wvrt_ctz((uint64_t)(value >> 64), 64) : wvrt_ctz(low, 64);
}

// Each half contributes independently to a wide population count.
int32_t wvrt_popcnt128(wvrt_u128 value) {
	return wvrt_popcnt((uint64_t)(value >> 64), 64) + wvrt_popcnt((uint64_t)value, 64);
}
#endif

// Interpret already masked bits using only representable signed casts.
static int64_t wvrt_signed_word(uint64_t bits, uint64_t mask) {
	if (bits <= (mask >> 1)) { return (int64_t)bits; }
	return -INT64_C(1) - (int64_t)(mask - bits);
}

// Check the width before either rotation component can shift.
static void wvrt_rotate_count(int32_t wvrtcount, int32_t width,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// A failed rotation points at its count operand.
	if (!wvrt_valid_shift_count(wvrtcount, width)) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "rotate count out of range");
	}
}

// Rotate the exact-width bits and report invalid counts at the count operand.
uint64_t wvrt_rotate_at_span(uint64_t wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t width, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_rotate_count(wvrtcount, width, wvrtfile, wvrtstart, wvrtend);
	const uint64_t mask = wvrt_word_mask(width);
	const uint64_t bits = wvrtvalue & mask;
	if (wvrtcount == 0) { return bits; }
	const int32_t right = wvrtright ? wvrtcount : width - wvrtcount;
	return ((bits >> right) | (bits << (width - right))) & mask;
}

// Recover a signed result from the same rotated bit pattern.
int64_t wvrt_signed_rotate_at_span(int64_t wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t width, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	const uint64_t bits = wvrt_rotate_at_span((uint64_t)wvrtvalue, wvrtcount, wvrtright,
		width, wvrtfile, wvrtstart, wvrtend);
	return wvrt_signed_word(bits, wvrt_word_mask(width));
}

// Preserve the direct right-rotate entry for a full unsigned word.
uint64_t wvrt_u64_rotr_at_span(uint64_t wvrtvalue, int32_t wvrtcount,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_rotate_at_span(wvrtvalue, wvrtcount, true, 64, wvrtfile, wvrtstart, wvrtend);
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

// Shift an exact-width signed value using unsigned bits and in-range signed casts.
int64_t wvrt_signed_shift_at_span(int64_t wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t width, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrtcount = wvrt_shift_count_at_span(wvrtcount, width, wvrtfile, wvrtstart, wvrtend);
	// Reduce a left shift to its declared width before interpreting its sign bit.
	if (!wvrtright) {
		const uint64_t mask = wvrt_word_mask(width);
		const uint64_t bits = ((uint64_t)wvrtvalue << wvrtcount) & mask;
		return wvrt_signed_word(bits, mask);
	}
	// Shift the complemented magnitude to extend the sign of a negative word.
	if (wvrtvalue < 0) {
		const uint64_t inverted = (uint64_t)(-INT64_C(1) - wvrtvalue);
		return -INT64_C(1) - (int64_t)(inverted >> wvrtcount);
	}
	return (int64_t)((uint64_t)wvrtvalue >> wvrtcount);
}

// Preserve the direct 32-bit shift entry used by generated C.
int32_t wvrt_i32_shift_at_span(int32_t wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return (int32_t)wvrt_signed_shift_at_span(wvrtvalue, wvrtcount, wvrtright, 32,
		wvrtfile, wvrtstart, wvrtend);
}

#ifdef WVRT_HAS_INT128
// Check a full-width unsigned shift before using the native unsigned operation.
wvrt_u128 wvrt_u128_shift_at_span(wvrt_u128 wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrtcount = wvrt_shift_count_at_span(wvrtcount, 128, wvrtfile, wvrtstart, wvrtend);
	return wvrtright ? wvrtvalue >> wvrtcount : wvrtvalue << wvrtcount;
}

// Shift unsigned bits and explicitly extend the sign of a negative right shift.
wvrt_i128 wvrt_i128_shift_at_span(wvrt_i128 wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrtcount = wvrt_shift_count_at_span(wvrtcount, 128, wvrtfile, wvrtstart, wvrtend);
	const wvrt_u128 wvrtbits = (wvrt_u128)wvrtvalue;
	if (!wvrtright) { return wvrt_i128_from_wrapped_bits(wvrtbits << wvrtcount); }
	if (wvrtvalue < 0) { return wvrt_i128_from_wrapped_bits(~(~wvrtbits >> wvrtcount)); }
	return (wvrt_i128)(wvrtbits >> wvrtcount);
}

// Rotate every bit in the native 128-bit unsigned representation.
wvrt_u128 wvrt_u128_rotate_at_span(wvrt_u128 wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_rotate_count(wvrtcount, 128, wvrtfile, wvrtstart, wvrtend);
	if (wvrtcount == 0) { return wvrtvalue; }
	const int32_t right = wvrtright ? wvrtcount : 128 - wvrtcount;
	return (wvrtvalue >> right) | (wvrtvalue << (128 - right));
}

// Return the rotated bit pattern as a value of the original signed type.
wvrt_i128 wvrt_i128_rotate_at_span(wvrt_i128 wvrtvalue, int32_t wvrtcount, bool wvrtright,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_i128_from_wrapped_bits(wvrt_u128_rotate_at_span((wvrt_u128)wvrtvalue,
		wvrtcount, wvrtright, wvrtfile, wvrtstart, wvrtend));
}
#endif

// Add in the modulo domain and return the corresponding signed value.
int32_t wvrt_i32_add_wrapping(int32_t wvrtleft, int32_t wvrtright) {
	return wvrt_i32_from_wrapped_bits(wvrt_u32_add_wrapping((uint32_t)wvrtleft, (uint32_t)wvrtright));
}

// Subtract in the modulo domain and return the corresponding signed value.
int32_t wvrt_i32_sub_wrapping(int32_t wvrtleft, int32_t wvrtright) {
	return wvrt_i32_from_wrapped_bits(wvrt_u32_sub_wrapping((uint32_t)wvrtleft, (uint32_t)wvrtright));
}

// Report integer arithmetic overflow at the checked source operation.
_Noreturn void wvrt_int_arithmetic_trap(int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "integer arithmetic overflow");
}

// Report the reason checked division failed at its original operation.
_Noreturn void wvrt_int_division_trap(bool wvrtzero, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	// Remainder and quotient share the zero-divisor diagnostic.
	if (wvrtzero) {
		wvrt_trap_at_span(wvrtfile, wvrtstart, wvrtend, "division by zero");
	}
	wvrt_int_arithmetic_trap(wvrtfile, wvrtstart, wvrtend);
}

// Keep the existing i32 call entry while using the same guarded operation as other widths.
int32_t wvrt_i32_divide_at_span(int32_t wvrtleft, int32_t wvrtright, bool wvrtremainder,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend) {
	return wvrt_int_divide_at_span(int32_t, wvrtleft, wvrtright, wvrtremainder, wvrtfile, wvrtstart, wvrtend);
}

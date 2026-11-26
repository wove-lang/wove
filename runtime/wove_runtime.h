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

// Exact native floating representations and checked numerical conversions.
float wvrt_f32_from_bits(uint32_t wvrtbits);
uint32_t wvrt_f32_to_bits(float wvrtvalue);
double wvrt_f64_from_bits(uint64_t wvrtbits);
uint64_t wvrt_f64_to_bits(double wvrtvalue);
double wvrt_float_to_integer(double wvrtvalue, double wvrtlower, double wvrtupper,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
float wvrt_f32_narrow(double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);

// Half operations use the native binary16 ABI when the C target exposes it.
#if defined(__FLT16_MANT_DIG__) && __FLT16_MANT_DIG__ == 11
#define WVRT_HAS_F16 1
_Float16 wvrt_f16_from_bits(uint16_t wvrtbits);
uint16_t wvrt_f16_to_bits(_Float16 wvrtvalue);
_Float16 wvrt_f16_narrow(double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
#endif

// Extended operations use the native long double ABI with a 64-bit significand.
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 64
#define WVRT_HAS_F80 1
long double wvrt_f80_to_integer(long double wvrtvalue, long double wvrtlower, long double wvrtupper,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
double wvrt_f80_to_f64(long double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
float wvrt_f80_to_f32(long double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
// The half destination also needs the target's binary16 ABI.
#ifdef WVRT_HAS_F16
_Float16 wvrt_f80_to_f16(long double wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
#endif
#endif

// Choose a binary128 type and literal suffix that match the target's native ABI.
#if defined(__LDBL_MANT_DIG__) && __LDBL_MANT_DIG__ == 113
typedef long double wvrt_f128;
#define WVRT_F128_LITERAL(wvrt_literal) wvrt_literal##L
#define WVRT_HAS_F128 1
#elif defined(__SIZEOF_FLOAT128__)
typedef __float128 wvrt_f128;
#define WVRT_F128_LITERAL(wvrt_literal) wvrt_literal##Q
#define WVRT_HAS_F128 1
#endif

// Floating narrowing and integer checks retain the native binary128 operand.
#ifdef WVRT_HAS_F128
wvrt_f128 wvrt_f128_to_integer(wvrt_f128 wvrtvalue, wvrt_f128 wvrtlower, wvrt_f128 wvrtupper,
	int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
double wvrt_f128_to_f64(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
float wvrt_f128_to_f32(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
// A target may support binary128 alongside x87 extended precision or binary16.
#ifdef WVRT_HAS_F80
long double wvrt_f128_to_f80(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
#endif
#ifdef WVRT_HAS_F16
_Float16 wvrt_f128_to_f16(wvrt_f128 wvrtvalue, int32_t wvrtfile, int32_t wvrtstart, int32_t wvrtend);
#endif
#endif

int wvrt_run_host_entry(int argc, char **argv, size_t span_size,
	void (*write_span)(void *, int32_t, int32_t, int32_t), int32_t (*entry)(wvrt_slice, wvrt_slice));

// Bootstrap-only file and stream operations implemented outside the shipped runtime.
int32_t wvrt_bootstrap_canonical_path(wvrt_slice path, wvrt_mut_slice out);
int32_t wvrt_bootstrap_file(wvrt_slice path, wvrt_mut_slice out);
bool wvrt_bootstrap_same_file(wvrt_slice left, wvrt_slice right);
bool wvrt_bootstrap_write(wvrt_slice bytes, bool diagnostic);
uint8_t wvrt_bootstrap_os(void);
uint8_t wvrt_bootstrap_arch(void);

// Store each row as the generated Wove type before exposing the two borrowed views.
#define WVRT_HOST_ENTRY(entry, span) \
	static void entry ## _arg_span(void *items, int32_t index, int32_t start, int32_t end) { \
		((span *)items)[index] = (span){start, end}; \
	} \
	int main(int argc, char **argv) { \
		return wvrt_run_host_entry(argc, argv, sizeof(span), entry ## _arg_span, entry); \
	}

typedef struct {
	wvrt_slice wvrt_view;
	bool wvrt_checked;
	int32_t wvrt_file;
	int32_t wvrt_start;
	int32_t wvrt_end;
	const char *wvrt_value;
} wvrt_cstr_argument;

char *wvrt_cstr_prepare(wvrt_cstr_argument *wvrt_arguments, size_t wvrt_count);
void wvrt_cstr_copy_back(wvrt_mut_slice wvrt_view, const char *wvrt_value);
void wvrt_cstr_drop(char *wvrt_storage);
char *const *wvrt_cstrs_prepare(wvrt_slice wvrt_rows, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_cstrs_drop(char *const *wvrt_table);

void wvrt_check_c_bytes(wvrt_slice wvrt_view, int64_t wvrt_size, int64_t wvrt_count,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

void wvrt_check_c_array(wvrt_slice wvrt_view, size_t wvrt_element_size, int64_t wvrt_count,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

// Test the sign without converting unsigned values into a signed domain.
#define WVRT_INTEGER_NEGATIVE(value) ((value) <= 0 && (value) != 0)

// Reduce a native product to the slice's i32 domain while retaining failure priority.
#define wvrt_check_c_bytes_native(view, size, count, file, start, end) \
	__extension__ ({ \
		__auto_type wvrt_size = (size); \
		__auto_type wvrt_count = (count); \
		int32_t wvrt_product; \
		int64_t wvrt_extent; \
		/* A negative factor is invalid even when the product would be zero. */ \
		if (WVRT_INTEGER_NEGATIVE(wvrt_size) || WVRT_INTEGER_NEGATIVE(wvrt_count)) { \
			wvrt_extent = -1; \
		/* A product above INT32_MAX exceeds every valid slice length. */ \
		} else if (__builtin_mul_overflow(wvrt_size, wvrt_count, &wvrt_product)) { \
			wvrt_extent = (int64_t)INT32_MAX + 1; \
		/* Forward a fitting product to the shared slice check. */ \
		} else { \
			wvrt_extent = wvrt_product; \
		} \
		wvrt_check_c_bytes((view), 1, wvrt_extent, (file), (start), (end)); \
	})

// Forward a named count without narrowing or reinterpreting its signedness.
#define wvrt_check_c_array_native(view, element_size, count, file, start, end) \
	__extension__ ({ \
		__auto_type wvrt_count = (count); \
		int32_t wvrt_checked; \
		int64_t wvrt_extent; \
		/* Preserve the original count's sign before reducing to slice length. */ \
		if (WVRT_INTEGER_NEGATIVE(wvrt_count)) { \
			wvrt_extent = -1; \
		/* A count above INT32_MAX exceeds every supported slice length. */ \
		} else if (__builtin_add_overflow(wvrt_count, 0, &wvrt_checked)) { \
			wvrt_extent = (int64_t)INT32_MAX + 1; \
		/* The reduced count fits in the runtime's length domain. */ \
		} else { \
			wvrt_extent = wvrt_checked; \
		} \
		wvrt_check_c_array((view), (element_size), wvrt_extent, (file), (start), (end)); \
	})

void wvrt_check_c_handle(const volatile void *wvrt_handle, int32_t wvrt_file,
	int32_t wvrt_start, int32_t wvrt_end);
void wvrt_check_c_handle_empty(const volatile void *wvrt_handle, int32_t wvrt_file,
	int32_t wvrt_start, int32_t wvrt_end);

typedef struct wvrt_c_storage wvrt_c_storage;

wvrt_c_storage *wvrt_c_storage_new(size_t wvrt_size, size_t wvrt_alignment,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_c_storage_access(wvrt_c_storage *wvrt_storage, _Bool wvrt_initializing,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void *wvrt_c_storage_data(wvrt_c_storage *wvrt_storage);
_Bool wvrt_c_storage_active(const wvrt_c_storage *wvrt_storage);
void wvrt_c_storage_finish_init(wvrt_c_storage *wvrt_storage, int32_t wvrt_status);
void wvrt_c_storage_free(wvrt_c_storage *wvrt_storage);

// Transfer one owning array pointer and leave the original slot empty.
static inline wvrt_array *wvrt_array_take(wvrt_array **wvrt_slot) {
	wvrt_array *wvrt_result = *wvrt_slot;
	*wvrt_slot = NULL;
	return wvrt_result;
}

// Borrow the full owner as a shared view over its existing storage.
static inline wvrt_slice wvrt_array_borrow_shared(const wvrt_array *wvrtbuffer) {
	return (wvrt_slice){wvrtbuffer->wvrt_items, wvrtbuffer->wvrt_length, wvrtbuffer->wvrt_element_size};
}

// Borrow the full owner as a writable view over its existing storage.
static inline wvrt_mut_slice wvrt_array_borrow_mut(wvrt_array *wvrtbuffer) {
	return (wvrt_mut_slice){wvrtbuffer->wvrt_items, wvrtbuffer->wvrt_length, wvrtbuffer->wvrt_element_size};
}

// Reborrow the same byte range with read-only access.
static inline wvrt_slice wvrt_share_mut_slice(wvrt_mut_slice wvrtview) {
	return (wvrt_slice){wvrtview.wvrt_items, wvrtview.wvrt_length, wvrtview.wvrt_element_size};
}

wvrt_array *wvrt_array_new(int32_t wvrt_length, size_t wvrt_element_size,
	size_t wvrt_element_alignment, wvrt_location wvrt_site);
wvrt_array *wvrt_array_new_at_span(int32_t wvrt_length, size_t wvrt_element_size, size_t wvrt_element_alignment,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_array_new_u8_at_span(int32_t wvrt_length, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_array_new_rows_at_span(int32_t wvrt_length, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_slice wvrt_array_borrow_row_at_span(wvrt_slice wvrt_rows, int32_t wvrt_index,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_array_store_row_at_span(wvrt_mut_slice wvrt_rows, int32_t wvrt_index, wvrt_array *wvrt_row,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_array_copy_u8_at_span(const void *wvrt_bytes, int32_t wvrt_length,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_snapshot_borrowed_cstr_at_span(const char *wvrt_text,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_snapshot_bounded_cstr_at_span(const char *wvrt_text, size_t wvrt_capacity,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

// Capture the record pointer once and use the native header's complete field type and size.
#define WVRT_SNAPSHOT_CSTR_FIELD_AT_SPAN(result, field, file, start, end) \
	__extension__ ({ \
		__auto_type wvrt_record = (result); \
		_Static_assert(sizeof(wvrt_record->field) > 0 && _Generic(&(wvrt_record->field), \
			char (*)[sizeof(wvrt_record->field)]: 1, const char (*)[sizeof(wvrt_record->field)]: 1, default: 0), \
			"Wove C snapshot field must be a nonempty inline char array at file " #file " bytes [" #start "," #end ")"); \
		wvrt_record == NULL ? NULL : wvrt_snapshot_bounded_cstr_at_span( \
			wvrt_record->field, sizeof(wvrt_record->field), (file), (start), (end)); \
	})

// readdir guarantees a terminated name even when the returned record extends the declared array.
#define WVRT_READDIR_NAME(directory) \
	__extension__ ({ \
		__auto_type wvrt_entry = readdir(directory); \
		wvrt_entry == NULL ? NULL : (const char *)wvrt_entry->d_name; \
	})

wvrt_array *wvrt_snapshot_cstr_at_span(char *wvrt_text, void (*wvrt_release)(void *),
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_snapshot_chars_at_span(char *wvrt_text, void (*wvrt_release)(char *),
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_snapshot_cstrs_at_span(char *const *wvrt_table,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_require_array_at_span(wvrt_array *wvrt_value, int32_t wvrt_file,
	int32_t wvrt_start, int32_t wvrt_end);
void wvrt_array_drop(wvrt_array *wvrt_buffer);
bool wvrt_array_try_reserve(wvrt_array *wvrt_buffer, int32_t wvrt_capacity);
void wvrt_array_reserve_at_span(wvrt_array *wvrt_buffer, int32_t wvrt_capacity,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_array_push_at_span(wvrt_array *wvrt_buffer, const void *wvrt_value,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_array_remove_at_span(wvrt_array *wvrt_buffer, int32_t wvrt_index, void *wvrt_result,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_array_remove_row_at_span(wvrt_array *wvrt_buffer, int32_t wvrt_index,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_array_take_row_at_span(wvrt_array *wvrt_buffer, int32_t wvrt_index,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_array_truncate_at_span(wvrt_array *wvrt_buffer, int32_t wvrt_length,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

// Return the removed value with its exact element type, including aggregate types.
#define wvrt_array_remove_value(type, owner, index, file, start, end) \
	__extension__ ({ \
		type wvrt_removed; \
		wvrt_array_remove_at_span((owner), (index), &wvrt_removed, (file), (start), (end)); \
		wvrt_removed; \
	})
wvrt_slice wvrt_slice_range_u8(const void *wvrt_items, int32_t wvrt_length,
	int32_t wvrt_start, int32_t wvrt_end, wvrt_location wvrt_site);
wvrt_slice wvrt_slice_range_at_span(wvrt_slice wvrt_view, int32_t wvrt_start, int32_t wvrt_end,
	int32_t wvrt_file, int32_t wvrt_span_start, int32_t wvrt_span_end);
wvrt_slice wvrt_slice_range_u8_at_span(wvrt_slice wvrt_view, int32_t wvrt_start, int32_t wvrt_end,
	int32_t wvrt_file, int32_t wvrt_span_start, int32_t wvrt_span_end);
wvrt_mut_slice wvrt_mut_slice_range_u8(void *wvrt_items, int32_t wvrt_length,
	int32_t wvrt_start, int32_t wvrt_end, wvrt_location wvrt_site);
wvrt_mut_slice wvrt_mut_slice_range_at_span(wvrt_mut_slice wvrt_view, int32_t wvrt_start, int32_t wvrt_end,
	int32_t wvrt_file, int32_t wvrt_span_start, int32_t wvrt_span_end);
wvrt_mut_slice wvrt_mut_slice_range_u8_at_span(wvrt_mut_slice wvrt_view, int32_t wvrt_start, int32_t wvrt_end,
	int32_t wvrt_file, int32_t wvrt_span_start, int32_t wvrt_span_end);
void wvrt_check_slice_extent_at_span(int32_t wvrt_length, int32_t wvrt_expected,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
size_t wvrt_slice_offset(int32_t wvrt_length, int32_t wvrt_index, wvrt_location wvrt_site);
size_t wvrt_slice_offset_at_span(int32_t wvrt_length, int32_t wvrt_index,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
size_t wvrt_array_index_at_span(int32_t wvrt_length, int32_t wvrt_index,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint8_t wvrt_slice_load_u8(const void *wvrt_items, int32_t wvrt_length, int32_t wvrt_index,
	wvrt_location wvrt_site);
uint8_t wvrt_slice_load_u8_at_span(wvrt_slice wvrt_view, int32_t wvrt_index,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_slice_store_u8(void *wvrt_items, int32_t wvrt_length, int32_t wvrt_index,
	uint8_t wvrt_value, wvrt_location wvrt_site);
void wvrt_slice_store_u8_at_span(wvrt_mut_slice wvrt_view, int32_t wvrt_index, uint8_t wvrt_value,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int64_t wvrt_int_convert(int64_t wvrt_value, int64_t wvrt_min, int64_t wvrt_max,
	wvrt_location wvrt_site);
_Noreturn void wvrt_int_conversion_trap(int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint64_t wvrt_int_from_bytes(wvrt_slice wvrt_view, int32_t wvrt_width, bool wvrt_big_endian,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int64_t wvrt_signed_from_bytes(wvrt_slice wvrt_view, int32_t wvrt_width, bool wvrt_big_endian,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

// Use the target compiler's integer range, including aliases wider than int64_t.
#define wvrt_int_to_native(type, value, file, start, end) \
	__extension__ ({ \
		/* Overflow builtins cannot store a boolean. Check the 0..1 range explicitly. */ \
		__typeof__(_Generic((type)0, _Bool: (unsigned char)0, default: (type)0)) wvrt_result; \
		/* Addition of zero checks representability without first narrowing the value. */ \
		if (__builtin_add_overflow((value), 0, &wvrt_result) || \
			_Generic((type)0, _Bool: wvrt_result > 1, default: false)) { \
			wvrt_int_conversion_trap((file), (start), (end)); \
		} \
		(type)wvrt_result; \
	})

// Test representability without narrowing the input or touching caller storage.
#define wvrt_int_fits_type(type, value) (!__builtin_add_overflow((value), 0, &(type){0}))

_Noreturn void wvrt_int_arithmetic_trap(int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

// The overflow builtin supplies the modulo result in the exact destination type.
#define wvrt_int_wrapping(check, type, left, right) \
	__extension__ ({ \
		type wvrt_result; \
		(void)check((left), (right), &wvrt_result); \
		wvrt_result; \
	})

// A checked result uses the same exact-width builtin and reports overflow at the head.
#define wvrt_int_checked(check, type, left, right, file, start, end) \
	__extension__ ({ \
		type wvrt_result; \
		/* Test representability before exposing the result. */ \
		if (check((left), (right), &wvrt_result)) { \
			wvrt_int_arithmetic_trap((file), (start), (end)); \
		} \
		wvrt_result; \
	})

// Let the compiler check the exact pointee width and publish only a valid result.
#define wvrt_int_try_overflow(check, left, right, output) \
	__extension__ ({ \
		__typeof__(*(output)) wvrt_left = (left); \
		__typeof__(*(output)) wvrt_right = (right); \
		__typeof__(*(output)) *wvrt_output = (output); \
		__typeof__(*wvrt_output) wvrt_value; \
		bool wvrt_ok = !check(wvrt_left, wvrt_right, &wvrt_value); \
		/* An overflow builtin may write a truncated result on failure. */ \
		if (wvrt_ok) { \
			*wvrt_output = wvrt_value; \
		} \
		wvrt_ok; \
	})

// Return the selected signed minimum, or zero for an unsigned integer.
#define wvrt_int_signed_min(value) \
	_Generic((value), int8_t: INT8_MIN, int16_t: INT16_MIN, \
		int32_t: INT32_MIN, int64_t: INT64_MIN, default: 0)

// Avoid undefined signed arithmetic and leave the destination alone on failure.
#define wvrt_int_try_divide(left, right, output, remainder) \
	__extension__ ({ \
		__typeof__(*(output)) wvrt_left = (left); \
		__typeof__(*(output)) wvrt_right = (right); \
		__typeof__(*(output)) *wvrt_output = (output); \
		__typeof__(*wvrt_output) wvrt_min = wvrt_int_signed_min(wvrt_left); \
		bool wvrt_remainder = (remainder); \
		bool wvrt_ok = false; \
		/* Check the divisor before either C operator can run. */ \
		if (wvrt_right != 0) { \
			bool wvrt_overflow = wvrt_min != 0 && wvrt_left == wvrt_min && wvrt_right == (__typeof__(wvrt_right))-1; \
			/* The signed minimum remainder is zero without executing the C operation. */ \
			if (!wvrt_overflow || wvrt_remainder) { \
				*wvrt_output = (__typeof__(*wvrt_output))(wvrt_overflow ? 0 : \
					(wvrt_remainder ? wvrt_left % wvrt_right : wvrt_left / wvrt_right)); \
				wvrt_ok = true; \
			} \
		} \
		wvrt_ok; \
	})

_Noreturn void wvrt_int_division_trap(bool wvrt_zero, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

// Use the recoverable guard and convert a failed result into a source-site trap.
#define wvrt_int_divide_at_span(type, left, right, remainder, file, start, end) \
	__extension__ ({ \
		type wvrt_dividend = (left); \
		type wvrt_divisor = (right); \
		type wvrt_division_result; \
		/* The shared guard leaves the result untouched on zero or quotient overflow. */ \
		if (!wvrt_int_try_divide(wvrt_dividend, wvrt_divisor, &wvrt_division_result, (remainder))) { \
			wvrt_int_division_trap(wvrt_divisor == 0, (file), (start), (end)); \
		} \
		wvrt_division_result; \
	})

uint8_t wvrt_int_to_u8(int64_t wvrt_value, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_int_to_i32(int64_t wvrt_value, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint32_t wvrt_int_to_u32(int64_t wvrt_value, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_i32_from_wrapped_bits(uint32_t wvrt_bits);
int64_t wvrt_i64_from_wrapped_bits(uint64_t wvrt_bits);
int64_t wvrt_signed_shift_at_span(int64_t wvrt_value, int32_t wvrt_count, bool wvrt_right,
	int32_t width, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_i32_shift_at_span(int32_t wvrt_value, int32_t wvrt_count, bool wvrt_right,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_i32_add_wrapping(int32_t wvrt_left, int32_t wvrt_right);
int32_t wvrt_i32_sub_wrapping(int32_t wvrt_left, int32_t wvrt_right);
int32_t wvrt_i32_divide_at_span(int32_t wvrt_left, int32_t wvrt_right, bool wvrt_remainder,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint32_t wvrt_u32_add_wrapping(uint32_t wvrt_left, uint32_t wvrt_right);
uint32_t wvrt_u32_sub_wrapping(uint32_t wvrt_left, uint32_t wvrt_right);
int32_t wvrt_u32_shift_count(int32_t wvrt_count, wvrt_location wvrt_site);
int32_t wvrt_shift_count_at_span(int32_t wvrt_count, int32_t width,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_u32_shift_count_at_span(int32_t wvrt_count, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_u64_shift_count_at_span(int32_t wvrt_count, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint64_t wvrt_rotate_at_span(uint64_t wvrt_value, int32_t wvrt_count, bool wvrt_right,
	int32_t width, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int64_t wvrt_signed_rotate_at_span(int64_t wvrt_value, int32_t wvrt_count, bool wvrt_right,
	int32_t width, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint64_t wvrt_u64_rotr_at_span(uint64_t wvrt_value, int32_t wvrt_count,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

#endif

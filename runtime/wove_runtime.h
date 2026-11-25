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

int wvrt_run_host_entry(int argc, char **argv, size_t span_size,
	void (*write_span)(void *, int32_t, int32_t, int32_t), int32_t (*entry)(wvrt_slice, wvrt_slice));

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

typedef struct wvrt_c_storage wvrt_c_storage;

wvrt_c_storage *wvrt_c_storage_new(size_t wvrt_size, size_t wvrt_alignment,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void wvrt_c_storage_access(wvrt_c_storage *wvrt_storage, _Bool wvrt_initializing,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
void *wvrt_c_storage_data(wvrt_c_storage *wvrt_storage);
_Bool wvrt_c_storage_active(const wvrt_c_storage *wvrt_storage);
void wvrt_c_storage_finish_init(wvrt_c_storage *wvrt_storage, int32_t wvrt_status);
void wvrt_c_storage_free(wvrt_c_storage *wvrt_storage);

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
wvrt_array *wvrt_snapshot_cstr_at_span(char *wvrt_text, void (*wvrt_release)(void *),
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_snapshot_chars_at_span(char *wvrt_text, void (*wvrt_release)(char *),
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_snapshot_cstrs_at_span(char *const *wvrt_table,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
wvrt_array *wvrt_require_array_at_span(wvrt_array *wvrt_value, int32_t wvrt_file,
	int32_t wvrt_start, int32_t wvrt_end);
void wvrt_array_drop(wvrt_array *wvrt_buffer);
wvrt_slice wvrt_slice_range_u8(const void *wvrt_items, int32_t wvrt_length,
	int32_t wvrt_start, int32_t wvrt_end, wvrt_location wvrt_site);
wvrt_slice wvrt_slice_range_u8_at_span(wvrt_slice wvrt_view, int32_t wvrt_start, int32_t wvrt_end,
	int32_t wvrt_file, int32_t wvrt_span_start, int32_t wvrt_span_end);
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

uint8_t wvrt_int_to_u8(int64_t wvrt_value, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_int_to_i32(int64_t wvrt_value, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint32_t wvrt_int_to_u32(int64_t wvrt_value, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_i32_add_wrapping(int32_t wvrt_left, int32_t wvrt_right);
int32_t wvrt_i32_sub_wrapping(int32_t wvrt_left, int32_t wvrt_right);
int32_t wvrt_i32_divide_at_span(int32_t wvrt_left, int32_t wvrt_right, bool wvrt_remainder,
	int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
uint32_t wvrt_u32_add_wrapping(uint32_t wvrt_left, uint32_t wvrt_right);
uint32_t wvrt_u32_sub_wrapping(uint32_t wvrt_left, uint32_t wvrt_right);
int32_t wvrt_u32_shift_count(int32_t wvrt_count, wvrt_location wvrt_site);
int32_t wvrt_u32_shift_count_at_span(int32_t wvrt_count, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);
int32_t wvrt_u64_shift_count_at_span(int32_t wvrt_count, int32_t wvrt_file, int32_t wvrt_start, int32_t wvrt_end);

#endif

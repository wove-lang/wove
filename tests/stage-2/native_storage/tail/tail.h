/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_NATIVE_TAIL_H
#define WOVE_TEST_NATIVE_TAIL_H

#include <errno.h>
#include <stdint.h>

struct test_handle {
	_Alignas(64) uint32_t count;
	int32_t kind;
	__extension__ unsigned char data[0];
};

enum { TEST_TAIL_SIZE = 80 };
static uintptr_t test_filled_address;

#ifdef WOVE_TRACK_TAIL
void test_register_tail(void *data);
#endif

/* Model the GNU sizing call and fill the last byte of the requested allocation. */
static inline int32_t test_encode(struct test_handle *handle, int32_t ordinary_error)
{
#ifdef WOVE_TRACK_TAIL
	test_register_tail(handle);
#endif
	if ((uintptr_t)handle % _Alignof(struct test_handle) != 0 || handle->kind != 0) {
		errno = EINVAL;
		return -1;
	}
	if (ordinary_error != 0) {
		errno = EIO;
		return -1;
	}
	if (handle->count < TEST_TAIL_SIZE) {
		handle->count = TEST_TAIL_SIZE;
		errno = EOVERFLOW;
		return -1;
	}
	handle->kind = 1024;
	handle->data[handle->count - 1] = 42;
	handle->count = TEST_TAIL_SIZE;
	test_filled_address = (uintptr_t)handle;
	return 0;
}

/* Preserve the actual mutable pointer prototype of the GNU open call. */
static inline int32_t test_use(struct test_handle *handle)
{
	if ((uintptr_t)handle != test_filled_address || handle->kind != 1024 || handle->count != TEST_TAIL_SIZE) {
		return -1;
	}
	return handle->data[TEST_TAIL_SIZE - 1];
}

/* Let a test simulate a successful loan that later reports a different size. */
static inline int32_t test_report(struct test_handle *handle, uint32_t count)
{
	handle->count = count;
	return 0;
}

/* Return the error set by the immediately preceding fixture call. */
static inline int32_t test_errno(void)
{
	return errno;
}

#endif

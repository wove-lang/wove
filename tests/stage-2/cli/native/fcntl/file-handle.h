/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FILE_HANDLE_H
#define WOVE_TEST_FILE_HANDLE_H

#include <fcntl.h>

/* Prepare a bounded native handle so the bad-descriptor test can call open independently. */
static inline int seed_file_handle(struct file_handle *handle)
{
	if (handle->handle_bytes != 4) {
		return -1;
	}
	handle->handle_type = 0;
	handle->f_handle[3] = 42;
	return 0;
}

#endif

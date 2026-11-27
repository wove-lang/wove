/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_DARWIN_FCNTL_RECORDS_H
#define WOVE_TEST_DARWIN_FCNTL_RECORDS_H

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>

_Static_assert(_Generic(&(fcntl), int (*)(int, int, ...): 1, default: 0), "Darwin fcntl prototype");
_Static_assert(sizeof(fstore_t) == 32 && _Alignof(fstore_t) == 8, "fstore layout");
_Static_assert(offsetof(fstore_t, fst_flags) == 0 && offsetof(fstore_t, fst_posmode) == 4 &&
    offsetof(fstore_t, fst_offset) == 8 && offsetof(fstore_t, fst_length) == 16 &&
    offsetof(fstore_t, fst_bytesalloc) == 24, "fstore field offsets");
_Static_assert(sizeof(struct log2phys) == 20 && _Alignof(struct log2phys) == 4, "packed log2phys layout");
_Static_assert(offsetof(struct log2phys, l2p_flags) == 0 && offsetof(struct log2phys, l2p_contigbytes) == 4 &&
    offsetof(struct log2phys, l2p_devoffset) == 12, "log2phys field offsets");

/* Compare the extended call against a native legacy query and restore the file position. */
static inline int native_log2phys_at(int fd, off_t position, struct log2phys *output)
{
	off_t previous = lseek(fd, 0, SEEK_CUR);
	/* Query the requested offset only after saving the original position. */
	if (previous < 0 || lseek(fd, position, SEEK_SET) != position) {
		return -1;
	}
	int result = fcntl(fd, F_LOG2PHYS, output);
	int failure = errno;
	/* Restore the original position before returning the native result. */
	if (lseek(fd, previous, SEEK_SET) != previous) {
		return -1;
	}
	errno = failure;
	return result;
}

#endif

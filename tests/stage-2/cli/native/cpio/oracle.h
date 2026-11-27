// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_CPIO_ORACLE_H
#define WOVE_TEST_CPIO_ORACLE_H

#include <cpio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Return the index of the first mode value that differs from the native header.
static inline int32_t fixture_cpio_modes(const int32_t *values)
{
	static const int32_t native[] = {
		C_IRGRP, C_IROTH, C_IRUSR, C_ISBLK, C_ISCHR, C_ISCTG, C_ISDIR,
		C_ISFIFO, C_ISGID, C_ISLNK, C_ISREG, C_ISSOCK, C_ISUID, C_ISVTX,
		C_IWGRP, C_IWOTH, C_IWUSR, C_IXGRP, C_IXOTH, C_IXUSR,
	};
	// Compare each macro independently of the other mode bits.
	for (size_t index = 0; index < sizeof(native) / sizeof(native[0]); ++index) {
		if (values[index] != native[index])
			return (int32_t)index + 1;
	}
	return 0;
}

// Compare the Wove byte view with the native spelling, excluding its terminator.
static inline bool fixture_cpio_magic(const unsigned char *bytes, int32_t length)
{
	return length == (int32_t)(sizeof(MAGIC) - 1)
		&& memcmp(bytes, MAGIC, sizeof(MAGIC) - 1) == 0;
}

#endif

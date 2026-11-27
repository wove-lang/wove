// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TAR_ORACLE_H
#define WOVE_TEST_TAR_ORACLE_H

#include <stdint.h>
#include <string.h>
#include <tar.h>

_Static_assert(TMAGLEN == sizeof(TMAGIC), "the magic field includes its C terminator");
_Static_assert(TVERSLEN == sizeof(TVERSION) - 1, "the version field has no terminator");

// Return the index of the first integer value that differs from the native header.
static inline int32_t fixture_tar_integers(const int32_t *values)
{
	static const int32_t native[] = {
		AREGTYPE, BLKTYPE, CHRTYPE, CONTTYPE, DIRTYPE, FIFOTYPE, LNKTYPE,
		REGTYPE, SYMTYPE, TGEXEC, TGREAD, TGWRITE, TMAGLEN, TOEXEC,
		TOREAD, TOWRITE, TSGID, TSUID, TUEXEC, TUREAD, TUWRITE, TVERSLEN,
	};
	// Compare the type codes, mode bits, and field widths separately.
	for (size_t index = 0; index < sizeof(native) / sizeof(native[0]); ++index) {
		if (values[index] != native[index])
			return (int32_t)index + 1;
	}
	return 0;
}

// Verify both Wove byte views against the native spellings without C terminators.
static inline int32_t fixture_tar_strings(const unsigned char *magic, int32_t magic_length,
	const unsigned char *version, int32_t version_length)
{
	// TMAGLEN counts the terminator, which is outside the shared view.
	if (magic_length != TMAGLEN - 1 || memcmp(magic, TMAGIC, TMAGLEN - 1) != 0)
		return 23;
	// TVERSLEN already gives the exact field width.
	if (version_length != TVERSLEN || memcmp(version, TVERSION, TVERSLEN) != 0)
		return 24;
	return 0;
}

#ifdef TSVTX
// Read the sticky bit only in profiles that provide the native macro.
static inline int32_t fixture_tar_sticky(void)
{
	return TSVTX;
}
#endif

#endif

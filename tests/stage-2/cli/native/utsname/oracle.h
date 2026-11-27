// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_UTSNAME_ORACLE_H
#define WOVE_TEST_UTSNAME_ORACLE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/utsname.h>

static struct utsname fixture_expected;

// Capture an ordinary C uname result and return its native status.
static inline int32_t fixture_capture_utsname(void)
{
	return uname(&fixture_expected);
}

// Check the Wove extents against the actual named C members before any reads.
static inline bool fixture_utsname_widths(int32_t sysname, int32_t nodename, int32_t release,
	int32_t version, int32_t machine)
{
	return sysname == (int32_t)sizeof(fixture_expected.sysname)
		&& nodename == (int32_t)sizeof(fixture_expected.nodename)
		&& release == (int32_t)sizeof(fixture_expected.release)
		&& version == (int32_t)sizeof(fixture_expected.version)
		&& machine == (int32_t)sizeof(fixture_expected.machine);
}

// Compare all bytes through the first terminator, with both reads bounded.
static inline bool fixture_utsname_byte(const char *field, size_t width, int32_t index, uint8_t byte)
{
	const char *end = memchr(field, 0, width);
	return index >= 0 && (size_t)index < width && end != NULL
		&& ((size_t)index > (size_t)(end - field) || byte == (uint8_t)field[index]);
}

// Return a distinct result for each mismatched native field.
static inline int32_t fixture_utsname_bytes(int32_t index, uint8_t sysname, uint8_t nodename,
	uint8_t release, uint8_t version, uint8_t machine)
{
	// Compare each projection to the corresponding member read directly in C.
	if (!fixture_utsname_byte(fixture_expected.sysname, sizeof(fixture_expected.sysname), index, sysname))
		return 4;
	// The terminator and every meaningful byte of the node name must match.
	if (!fixture_utsname_byte(fixture_expected.nodename, sizeof(fixture_expected.nodename), index, nodename))
		return 5;
	// The release is a separate member from the version string.
	if (!fixture_utsname_byte(fixture_expected.release, sizeof(fixture_expected.release), index, release))
		return 6;
	// The version can use the full native extent.
	if (!fixture_utsname_byte(fixture_expected.version, sizeof(fixture_expected.version), index, version))
		return 7;
	// The machine identifies the running system independently of the release.
	if (!fixture_utsname_byte(fixture_expected.machine, sizeof(fixture_expected.machine), index, machine))
		return 8;
	return 0;
}

#ifdef SYS_NMLN
// Read the extension directly from the selected C header.
static inline int32_t fixture_sys_nmln(void)
{
	return SYS_NMLN;
}
#endif

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_POLL_ORACLE_H
#define WOVE_TEST_POLL_ORACLE_H

#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Check each common event value using the C header as the oracle.
static inline bool fixture_poll_events(const int32_t *values)
{
	static const int32_t native[] = {POLLERR, POLLHUP, POLLIN, POLLNVAL, POLLOUT, POLLPRI};
	// A mismatch identifies an incorrect C macro binding.
	for (size_t index = 0; index < sizeof(native) / sizeof(native[0]); ++index) {
		if (values[index] != native[index])
			return false;
	}
	return true;
}

#ifdef TEST_MEMBER
// Read the profile-dependent event directly from the selected header.
static inline int32_t fixture_poll_member(void)
{
	return TEST_MEMBER;
}
#endif

#endif

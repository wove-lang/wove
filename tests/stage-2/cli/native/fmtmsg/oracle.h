/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FMTMSG_ORACLE_H
#define WOVE_TEST_FMTMSG_ORACLE_H

#include <fmtmsg.h>
#include <stdint.h>

/* Compare each selected integer constant to the value from this C header. */
static inline int fixture_fmtmsg_values(const int32_t *values)
{
	static const int32_t native[] = {
		MM_HARD, MM_SOFT, MM_FIRM, MM_APPL, MM_UTIL, MM_OPSYS,
		MM_RECOVER, MM_NRECOV, MM_PRINT, MM_CONSOLE, MM_NOSEV, MM_HALT,
		MM_ERROR, MM_WARNING, MM_INFO, MM_NULLSEV, MM_OK, MM_NOMSG, MM_NOCON, MM_NOTOK,
	};
	/* Return the one-based constant index if it differs. */
	for (uint32_t index = 0; index < sizeof(native) / sizeof(native[0]); ++index) {
		if (values[index] != native[index])
			return (int)index + 1;
	}
	return 0;
}

#if defined(__GLIBC__) && defined(__USE_MISC)
/* Use static storage because the native severity registry borrows the string. */
static inline int fixture_register_severity(int severity)
{
	static const char name[] = "WOVE TEST";
	return addseverity(severity, name);
}
#endif

#endif

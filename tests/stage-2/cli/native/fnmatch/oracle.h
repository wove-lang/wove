/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FNMATCH_H
#define WOVE_TEST_FNMATCH_H

#include <fnmatch.h>

_Static_assert(_Generic(&fnmatch, int (*)(const char *, const char *, int): 1, default: 0),
	"native fnmatch prototype");

/* Invoke the SDK declaration with the same arguments supplied by the Wove fixture. */
static inline int fixture_fnmatch(const char *pattern, const char *name, int flags)
{
	return fnmatch(pattern, name, flags);
}

/* Return the header's selected value for each fixture constant. */
static inline int fixture_fnmatch_constant(int member)
{
	switch (member) {
	case 0: return FNM_PATHNAME;
	case 1: return FNM_NOESCAPE;
	case 2: return FNM_PERIOD;
	case 3: return FNM_NOMATCH;
#ifdef FNM_LEADING_DIR
	case 4: return FNM_LEADING_DIR;
#endif
#ifdef FNM_CASEFOLD
	case 5: return FNM_CASEFOLD;
#endif
#ifdef FNM_FILE_NAME
	case 6: return FNM_FILE_NAME;
#endif
#ifdef FNM_NOSYS
	case 7: return FNM_NOSYS;
#endif
#ifdef FNM_IGNORECASE
	case 8: return FNM_IGNORECASE;
#endif
#ifdef FNM_EXTMATCH
	case 9: return FNM_EXTMATCH;
#endif
	default: return -99;
	}
}

#endif

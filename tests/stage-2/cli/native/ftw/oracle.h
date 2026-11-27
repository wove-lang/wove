/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FTW_H
#define WOVE_TEST_FTW_H

#include <ftw.h>

/* Read the actual SDK's integer value for one selected visit constant or flag. */
static inline int fixture_ftw_constant(int member)
{
	switch (member) {
	case 0: return FTW_F;
	case 1: return FTW_D;
	case 2: return FTW_DNR;
	case 3: return FTW_NS;
#ifdef FTW_SL
	case 4: return FTW_SL;
#endif
#ifdef FTW_DP
	case 5: return FTW_DP;
#endif
#ifdef FTW_SLN
	case 6: return FTW_SLN;
#endif
#ifdef FTW_PHYS
	case 7: return FTW_PHYS;
#endif
#ifdef FTW_MOUNT
	case 8: return FTW_MOUNT;
#endif
#ifdef FTW_CHDIR
	case 9: return FTW_CHDIR;
#endif
#ifdef FTW_DEPTH
	case 10: return FTW_DEPTH;
#endif
#ifdef FTW_ACTIONRETVAL
	case 11: return FTW_ACTIONRETVAL;
#endif
#ifdef FTW_CONTINUE
	case 12: return FTW_CONTINUE;
#endif
#ifdef FTW_STOP
	case 13: return FTW_STOP;
#endif
#ifdef FTW_SKIP_SUBTREE
	case 14: return FTW_SKIP_SUBTREE;
#endif
#ifdef FTW_SKIP_SIBLINGS
	case 15: return FTW_SKIP_SIBLINGS;
#endif
	default: return -99;
	}
}

#ifdef FTW_PHYS
_Static_assert(sizeof(struct FTW) == 8 && _Alignof(struct FTW) == 4, "native FTW layout");
_Static_assert(__builtin_offsetof(struct FTW, base) == 0, "native FTW base");
_Static_assert(__builtin_offsetof(struct FTW, level) == 4, "native FTW level");

/* Produce an SDK record for the Wove field reads. */
static inline int fixture_ftw_write(struct FTW *record, int base, int level)
{
	record->base = base;
	record->level = level;
	return 0;
}

/* Read the basename offset from a record populated in Wove. */
static inline int fixture_ftw_base(const struct FTW *record)
{
	return record->base;
}

/* Read the traversal depth from a record populated in Wove. */
static inline int fixture_ftw_level(const struct FTW *record)
{
	return record->level;
}
#endif

#endif

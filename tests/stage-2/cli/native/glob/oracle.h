// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <glob.h>
#include <stdbool.h>
#include <stdint.h>

// Read each base flag and status from the active header.
static inline bool glob_core_ok(int32_t aborted, int32_t append, int32_t dooffs, int32_t err, int32_t mark, int32_t nocheck, int32_t noescape, int32_t nomatch, int32_t nosort, int32_t nospace, int32_t nosys) {
	return aborted == GLOB_ABORTED
		&& append == GLOB_APPEND
		&& dooffs == GLOB_DOOFFS
		&& err == GLOB_ERR
		&& mark == GLOB_MARK
		&& nocheck == GLOB_NOCHECK
		&& noescape == GLOB_NOESCAPE
		&& nomatch == GLOB_NOMATCH
		&& nosort == GLOB_NOSORT
		&& nospace == GLOB_NOSPACE
		&& nosys == GLOB_NOSYS;
}

#if defined(GLOB_ABEND) && defined(GLOB_ALTDIRFUNC) && defined(GLOB_BRACE) && defined(GLOB_MAGCHAR) && defined(GLOB_NOMAGIC) && defined(GLOB_TILDE)
// Check the extensions that Darwin and GNU feature profiles share.
static inline bool glob_common_ok(int32_t abend, int32_t altdirfunc, int32_t brace, int32_t magchar, int32_t nomagic, int32_t tilde) {
	return abend == GLOB_ABEND
		&& altdirfunc == GLOB_ALTDIRFUNC
		&& brace == GLOB_BRACE
		&& magchar == GLOB_MAGCHAR
		&& nomagic == GLOB_NOMAGIC
		&& tilde == GLOB_TILDE;
}

#if defined(__APPLE__)
// Read the Darwin-only flags and their alias through the same active header.
static inline bool glob_platform_ok(int32_t first, int32_t second, int32_t third) {
	return first == GLOB_LIMIT
		&& second == GLOB_MAXPATH
		&& third == GLOB_QUOTE;
}
#elif defined(__linux__)
// Read the GNU trailing-slash, leading-dot, and tilde checks.
static inline bool glob_platform_ok(int32_t first, int32_t second, int32_t third) {
	return first == GLOB_ONLYDIR
		&& second == GLOB_PERIOD
		&& third == GLOB_TILDE_CHECK;
}
#endif
#endif

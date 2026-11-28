// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_INTTYPES_FORMATS_ORACLE_H
#define WOVE_TEST_INTTYPES_FORMATS_ORACLE_H

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#define WOVE_PRI_ROW(width) { PRId##width, PRIi##width, PRIo##width, PRIu##width, PRIx##width, PRIX##width }

#define WOVE_SCN_ROW(width) { SCNd##width, SCNi##width, SCNo##width, SCNu##width, SCNx##width }

// Compare the exact bytes and length of one ordinary native format literal.
static inline bool fixture_inttypes_format(int32_t row, int32_t column, const unsigned char *bytes, int32_t length)
{
	static const char *const formats[][6] = {
		WOVE_PRI_ROW(8), WOVE_PRI_ROW(16), WOVE_PRI_ROW(32), WOVE_PRI_ROW(64),
		WOVE_PRI_ROW(LEAST8), WOVE_PRI_ROW(LEAST16), WOVE_PRI_ROW(LEAST32), WOVE_PRI_ROW(LEAST64),
		WOVE_PRI_ROW(FAST8), WOVE_PRI_ROW(FAST16), WOVE_PRI_ROW(FAST32), WOVE_PRI_ROW(FAST64),
		WOVE_PRI_ROW(PTR), WOVE_PRI_ROW(MAX),
		WOVE_SCN_ROW(8), WOVE_SCN_ROW(16), WOVE_SCN_ROW(32), WOVE_SCN_ROW(64),
		WOVE_SCN_ROW(LEAST8), WOVE_SCN_ROW(LEAST16), WOVE_SCN_ROW(LEAST32), WOVE_SCN_ROW(LEAST64),
		WOVE_SCN_ROW(FAST8), WOVE_SCN_ROW(FAST16), WOVE_SCN_ROW(FAST32), WOVE_SCN_ROW(FAST64),
		WOVE_SCN_ROW(PTR), WOVE_SCN_ROW(MAX),
	};
	const char *expected = formats[row][column];
	return length == (int32_t)strlen(expected) && memcmp(bytes, expected, (size_t)length) == 0;
}

#undef WOVE_SCN_ROW
#undef WOVE_PRI_ROW

#endif

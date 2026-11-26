// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
int32_t wgf274tbcmadd(int32_t, int32_t);
int32_t wgf277r2npsubtract(int32_t, int32_t);
_Bool wgf2los7kuless(int32_t, int32_t);
_Bool wgf0pgcvinequal(int32_t, int32_t);
_Bool wgf0hatnv8comparison_argument(void);
_Bool wgf3ks5s1gtruth(void);
_Bool wgf0t9n2jrfalsity(void);

// Report the cursor result, dynamic wrap boundaries, and the boolean helpers' values.
int main(int argc, char **argv) {
	return printf("%d %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %d %d %d %d %d %d %d\n",
		wove_emitted_main(argc, argv),
		wgf274tbcmadd(INT32_MAX, 1), wgf274tbcmadd(INT32_MIN, -1),
		wgf277r2npsubtract(INT32_MIN, 1), wgf277r2npsubtract(INT32_MAX, -1),
		wgf2los7kuless(-1, 0), wgf2los7kuless(0, -1),
		wgf0pgcvinequal(-1, -1), wgf0pgcvinequal(-1, 0),
		wgf0hatnv8comparison_argument(), wgf3ks5s1gtruth(), wgf0t9n2jrfalsity()) < 0;
}

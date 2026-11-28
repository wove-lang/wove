// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
int32_t wgf2_add(int32_t, int32_t);
int32_t wgf3_subtract(int32_t, int32_t);
_Bool wgf4_less(int32_t, int32_t);
_Bool wgf5_equal(int32_t, int32_t);
_Bool wgf7_comparison_argument(void);
_Bool wgf8_truth(void);
_Bool wgf9_falsity(void);

// Report the cursor result, dynamic wrap boundaries, and the boolean helpers' values.
int main(int argc, char **argv) {
	return printf("%d %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %d %d %d %d %d %d %d\n",
		wove_emitted_main(argc, argv),
		wgf2_add(INT32_MAX, 1), wgf2_add(INT32_MIN, -1),
		wgf3_subtract(INT32_MIN, 1), wgf3_subtract(INT32_MAX, -1),
		wgf4_less(-1, 0), wgf4_less(0, -1),
		wgf5_equal(-1, -1), wgf5_equal(-1, 0),
		wgf7_comparison_argument(), wgf8_truth(), wgf9_falsity()) < 0;
}

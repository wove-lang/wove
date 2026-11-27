// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>

#undef main

int wove_emitted_main(int argc, char **argv);
int32_t wgf0mvibr3add(int32_t, int32_t);
int32_t wgf25dh9jasubtract(int32_t, int32_t);
_Bool wgf1i9vtg9less(int32_t, int32_t);
_Bool wgf2egk39qequal(int32_t, int32_t);
_Bool wgf02rsqdhcomparison_argument(void);
_Bool wgf2tr005ptruth(void);
_Bool wgf3qqi0pafalsity(void);

// Report the cursor result, dynamic wrap boundaries, and the boolean helpers' values.
int main(int argc, char **argv) {
	return printf("%d %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %d %d %d %d %d %d %d\n",
		wove_emitted_main(argc, argv),
		wgf0mvibr3add(INT32_MAX, 1), wgf0mvibr3add(INT32_MIN, -1),
		wgf25dh9jasubtract(INT32_MIN, 1), wgf25dh9jasubtract(INT32_MAX, -1),
		wgf1i9vtg9less(-1, 0), wgf1i9vtg9less(0, -1),
		wgf2egk39qequal(-1, -1), wgf2egk39qequal(-1, 0),
		wgf02rsqdhcomparison_argument(), wgf2tr005ptruth(), wgf3qqi0pafalsity()) < 0;
}

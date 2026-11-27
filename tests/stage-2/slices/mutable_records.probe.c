// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	uint32_t wgm0000000;
	int32_t wgm0000001;
	_Bool wgm0000002;
	uint8_t wgm0000003;
} wgr1fpbp46Token;

#undef main
int wove_emitted_main(int argc, char **argv);
wgr1fpbp46Token wgf0o0aumnreplace_token(wvrt_mut_slice view, int32_t index, wgr1fpbp46Token value);

// Print every field of a copy or backing record without truncating its scalar values.
static void print_token(wgr1fpbp46Token value) {
	printf(" %" PRIu32 " %" PRId32 " %d %u", value.wgm0000000, value.wgm0000001,
		value.wgm0000002, (unsigned)value.wgm0000003);
}

// Give the emitted program the caller's original two-record buffer.
int main(int argc, char **argv) {
	wgr1fpbp46Token data[] = {
		{UINT32_C(2147483648), INT32_MAX, false, UINT8_C(17)},
		{UINT32_C(23), -7, false, UINT8_C(4)},
	};
	const wvrt_mut_slice full = {data, 2, sizeof *data};
	const wgr1fpbp46Token replacement = {UINT32_MAX, INT32_MIN, true, UINT8_MAX};

	// An invalid store must trap before touching inaccessible backing storage.
	if (argc == 2) {
		const wvrt_mut_slice inaccessible = {NULL, full.wvrt_length, sizeof *data};
		if (strcmp(argv[1], "negative") == 0) { (void)wgf0o0aumnreplace_token(inaccessible, -1, replacement); }
		if (strcmp(argv[1], "end") == 0) { (void)wgf0o0aumnreplace_token(inaccessible, full.wvrt_length, replacement); }
		return 2;
	}

	const wgr1fpbp46Token observed = wgf0o0aumnreplace_token(full, 1, replacement);
	printf("%d", wove_emitted_main(argc, argv));
	print_token(observed);
	print_token(data[0]);
	print_token(data[1]);
	putchar('\n');
	return 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

typedef struct {
	uint32_t wgm0_kind;
	int32_t wgm0_start;
	_Bool wgm0_ready;
	uint8_t wgm0_flag;
} wgr0_Token;

#undef main
int wove_emitted_main(int argc, char **argv);
wgr0_Token wgf1_forward_token(wvrt_slice view, int32_t index);
int32_t wgf2_token_count(wvrt_slice view);

// Pass typed C storage to the emitted slice readers and observe the copies.
int main(int argc, char **argv) {
	wgr0_Token data[] = {
		{UINT32_MAX, INT32_MIN, true, UINT8_MAX},
		{UINT32_C(2147483648), INT32_MAX, false, UINT8_C(17)},
	};
	const wvrt_slice full = {data, 2, sizeof *data};
	const wvrt_slice empty = {NULL, 0, sizeof *data};

	// An invalid index must trap even when the backing storage is inaccessible.
	if (argc == 2) {
		const wvrt_slice inaccessible = {NULL, full.wvrt_length, sizeof *data};
		if (strcmp(argv[1], "negative") == 0) { (void)wgf1_forward_token(inaccessible, -1); }
		if (strcmp(argv[1], "end") == 0) { (void)wgf1_forward_token(inaccessible, full.wvrt_length); }
		return 2;
	}

	const wgr0_Token first = wgf1_forward_token(full, 0);
	const wgr0_Token second = wgf1_forward_token(full, 1);
	data[0] = second;
	const wgr0_Token updated = wgf1_forward_token(full, 0);
	printf("%d %" PRId32 " %" PRId32 " %" PRIu32 " %" PRId32 " %d %u %" PRIu32 " %" PRId32 " %d %u %" PRIu32 "\n",
		wove_emitted_main(argc, argv), wgf2_token_count(full), wgf2_token_count(empty),
		first.wgm0_kind, first.wgm0_start, first.wgm0_ready, (unsigned)first.wgm0_flag,
		second.wgm0_kind, second.wgm0_start, second.wgm0_ready, (unsigned)second.wgm0_flag, updated.wgm0_kind);
	return 0;
}

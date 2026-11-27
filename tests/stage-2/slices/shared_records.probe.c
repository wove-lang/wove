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
} wgr1d5abitToken;

#undef main
int wove_emitted_main(int argc, char **argv);
wgr1d5abitToken wgf1kgk0p7forward_token(wvrt_slice view, int32_t index);
int32_t wgf1phblattoken_count(wvrt_slice view);

// Pass typed C storage to the emitted slice readers and observe the copies.
int main(int argc, char **argv) {
	wgr1d5abitToken data[] = {
		{UINT32_MAX, INT32_MIN, true, UINT8_MAX},
		{UINT32_C(2147483648), INT32_MAX, false, UINT8_C(17)},
	};
	const wvrt_slice full = {data, 2, sizeof *data};
	const wvrt_slice empty = {NULL, 0, sizeof *data};

	// An invalid index must trap even when the backing storage is inaccessible.
	if (argc == 2) {
		const wvrt_slice inaccessible = {NULL, full.wvrt_length, sizeof *data};
		if (strcmp(argv[1], "negative") == 0) { (void)wgf1kgk0p7forward_token(inaccessible, -1); }
		if (strcmp(argv[1], "end") == 0) { (void)wgf1kgk0p7forward_token(inaccessible, full.wvrt_length); }
		return 2;
	}

	const wgr1d5abitToken first = wgf1kgk0p7forward_token(full, 0);
	const wgr1d5abitToken second = wgf1kgk0p7forward_token(full, 1);
	data[0] = second;
	const wgr1d5abitToken updated = wgf1kgk0p7forward_token(full, 0);
	printf("%d %" PRId32 " %" PRId32 " %" PRIu32 " %" PRId32 " %d %u %" PRIu32 " %" PRId32 " %d %u %" PRIu32 "\n",
		wove_emitted_main(argc, argv), wgf1phblattoken_count(full), wgf1phblattoken_count(empty),
		first.wgm0000000, first.wgm0000001, first.wgm0000002, (unsigned)first.wgm0000003,
		second.wgm0000000, second.wgm0000001, second.wgm0000002, (unsigned)second.wgm0000003, updated.wgm0000000);
	return 0;
}

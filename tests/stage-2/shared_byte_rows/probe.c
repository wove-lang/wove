// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "probe.h"
#include "wove_runtime.h"

// The graph emitter names this ordinary Wove function by its declaration identity.
int32_t wgf18pdvlfcheck_rows(wvrt_slice rows, int32_t argc);

// Supply the nested descriptors while using the existing executable entry ABI.
int32_t probe_shared_byte_rows(int32_t argc) {
	const uint8_t first[] = {0, ' ', 255};
	const uint8_t last[] = {42};
	const wvrt_slice rows[] = {
		{first, sizeof(first), sizeof(uint8_t)},
		{NULL, 0, sizeof(uint8_t)},
		{last, sizeof(last), sizeof(uint8_t)},
	};
	const wvrt_slice view = argc >= 4
		? (wvrt_slice){NULL, 0, sizeof(wvrt_slice)}
		: (wvrt_slice){rows, sizeof(rows) / sizeof(rows[0]), sizeof(rows[0])};
	return wgf18pdvlfcheck_rows(view, argc);
}

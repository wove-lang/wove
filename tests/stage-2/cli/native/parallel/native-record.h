// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_RECORD_H
#define WOVE_PARALLEL_NATIVE_RECORD_H

struct fixture_detail {
	int amount;
	int alternate_amount;
};

struct fixture_record {
	unsigned long private_marker;
	struct fixture_detail detail;
	int extra;
};

#endif

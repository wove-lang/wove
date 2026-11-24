// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define main wove_fixture_main
#include "source_scan.c"
#undef main

// Match the seed's generated names to the scanner declarations.
#define source_offset wv_seed_fn_1
#define source_kind wv_seed_fn_2

_Static_assert(_Generic(&source_offset, int32_t (*)(wvrt_slice): 1, default: 0), "source offset type");
_Static_assert(_Generic(&source_kind, uint32_t (*)(wvrt_slice): 1, default: 0), "source kind type");

typedef struct {
	const char *label;
	const uint8_t *bytes;
	int32_t length;
	int32_t offset;
	uint32_t kind;
} ScanCase;

static const uint8_t whitespace[] = {32, 9, 13, 10};
static const uint8_t opening[] = {9, 32, 40};
static const uint8_t closing[] = {13, 41, 40};
static const uint8_t atom[] = {10, 120, 40};
static const uint8_t zero_byte[] = {0};
static const uint8_t high_byte[] = {255};
static const uint8_t parent[] = {120, 10, 32, 41, 120};

static const ScanCase cases[] = {
	{"empty", whitespace, 0, 0, 0},
	{"whitespace", whitespace, sizeof(whitespace), 4, 0},
	{"opening parenthesis", opening, sizeof(opening), 2, 40},
	{"closing parenthesis", closing, sizeof(closing), 1, 41},
	{"atom", atom, sizeof(atom), 1, 1},
	{"zero byte", zero_byte, sizeof(zero_byte), 0, 1},
	{"high byte", high_byte, sizeof(high_byte), 0, 1},
};

// Compare the full offset and kind returned by the compiled Wove scanner.
static void check(const char *label, wvrt_slice input, int32_t offset, uint32_t kind) {
	int32_t actual_offset = source_offset(input);
	uint32_t actual_kind = source_kind(input);
	// Show both return values for a failing source case.
	if (actual_offset != offset || actual_kind != kind) {
		fprintf(stderr, "%s: got offset %" PRId32 " and kind %" PRIu32
			", expected offset %" PRId32 " and kind %" PRIu32 "\n",
			label, actual_offset, actual_kind, offset, kind);
		exit(EXIT_FAILURE);
	}
}

// Exercise complete inputs and checked views into a larger buffer.
int main(void) {
	// Each complete input supplies its own logical byte length.
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		const ScanCase *test = &cases[i];
		wvrt_slice input = {test->bytes, test->length, sizeof(uint8_t)};
		check(test->label, input, test->offset, test->kind);
	}

	wvrt_location site = {__FILE__, __LINE__, 1};
	check("subrange", wvrt_slice_range_u8(parent, sizeof(parent), 1, 4, site), 2, 41);
	check("subrange end", wvrt_slice_range_u8(parent, sizeof(parent), 1, 3, site), 2, 0);
	return wove_fixture_main();
}

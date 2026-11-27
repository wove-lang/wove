/*
 * Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <libhistogram.h>
#include <limits.h>
#include <string.h>

enum { buckets = 256, output_bytes = buckets * 4, input_bytes = 66000 };

/* Compare every encoded count and the surrounding sentinels before testing rejection. */
static int check_histogram(void) {
	uint8_t input[input_bytes];
	/* Exercise every bucket, then cross the 8- and 16-bit boundaries in bucket 255. */
	for (size_t i = 0; i < buckets; ++i) { input[i] = (uint8_t)i; }
	memset(input + buckets, UINT8_MAX, sizeof(input) - buckets);
	uint8_t actual[output_bytes + 2];
	uint8_t expected[sizeof(actual)];
	memset(actual, 0xa5, sizeof(actual));
	memset(expected, 0, sizeof(expected));
	expected[0] = 0xa5;
	expected[sizeof(expected) - 1] = 0xa5;
	/* The complete wire representation is little-endian, including zero high bytes. */
	for (size_t bucket = 0; bucket < buckets; ++bucket) {
		uint32_t count = bucket == 255 ? input_bytes - buckets + 1 : 1;
		for (unsigned byte = 0; byte < 4; ++byte) {
			expected[1 + 4 * bucket + byte] = (uint8_t)(count >> (8u * byte));
		}
	}
	uint8_t *data = actual + 1;
	if (byte_histogram(input, sizeof(input), data, output_bytes) != WVRT_C_EXPORT_OK ||
		memcmp(actual, expected, sizeof(actual)) != 0) { return 1; }

	/* Both an incorrect exact extent and invalid pointer/count combinations leave all bytes alone. */
	if (byte_histogram(input, sizeof(input), data, output_bytes - 1) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		byte_histogram(input, sizeof(input), data, output_bytes + 1) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		byte_histogram(NULL, 1, data, output_bytes) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		byte_histogram(input, sizeof(input), NULL, output_bytes) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		byte_histogram(input, (size_t)INT32_MAX + 1, data, output_bytes) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		byte_histogram(input, SIZE_MAX, data, output_bytes) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		byte_histogram((const uint8_t *)(UINTPTR_MAX - 1), 2, data, output_bytes) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		byte_histogram(data + output_bytes - 1, 1, data, output_bytes) != WVRT_C_EXPORT_OVERLAP ||
		memcmp(actual, expected, sizeof(actual)) != 0) { return 2; }

	/* A zero-length null input still writes a complete all-zero histogram. */
	memset(expected + 1, 0, output_bytes);
	if (byte_histogram(NULL, 0, data, output_bytes) != WVRT_C_EXPORT_OK ||
		memcmp(actual, expected, sizeof(actual)) != 0) { return 3; }
	return 0;
}

/* Check strided overlap, alignment, scalar result aliases, and validation before writes. */
static int check_words(void) {
	uint32_t words[] = { 2, 4, 0x61, 0x62 };
	const uint32_t before[] = { 2, 4, 0x61, 0x62 };
	uint32_t result = 123;
	const uint32_t *misaligned_input = (const uint32_t *)((uintptr_t)words + 1);
	uint32_t *misaligned_output = (uint32_t *)((uintptr_t)words + 1);
	uint32_t *end_of_address_space = (uint32_t *)(UINTPTR_MAX - (sizeof(uint32_t) - 1));
	/* The result and borrowed storage are untouched for every rejected call. */
	if (bias_words(words, 1, 5, words + 2, 2, &result) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		bias_words(words, 2, 5, words + 2, 2, NULL) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		bias_words(misaligned_input, 2, 5, words + 2, 2, &result) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		bias_words(words, 2, 5, misaligned_output, 2, &result) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		bias_words(words, 2, 5, words + 2, 2, misaligned_output) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		bias_words(words, 2, 5, words + 2, 2, end_of_address_space) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		bias_words(words, 2, 5, words + 1, 2, &result) != WVRT_C_EXPORT_OVERLAP ||
		result != 123 || memcmp(words, before, sizeof(words)) != 0) { return 4; }

	/* Touching ranges are valid. The result aliases the first output word after the callee writes it. */
	if (bias_words(words, 2, 5, words + 2, 2, words + 2) != WVRT_C_EXPORT_OK ||
		words[0] != 2 || words[1] != 4 || words[2] != 16 || words[3] != 9) { return 5; }

	/* The same result can alias shared input if its backing storage is writable. */
	if (bias_words(words, 2, 5, words + 2, 2, words) != WVRT_C_EXPORT_OK ||
		words[0] != 16 || words[1] != 4 || words[2] != 7 || words[3] != 9) { return 6; }

	int32_t count = -1;
	/* Shared/shared overlap is legal, and empty shared ranges can have null addresses. */
	if (shared_count(words, 2, words + 1, 2, &count) != WVRT_C_EXPORT_OK || count != 4 ||
		shared_count(NULL, 0, NULL, 0, &count) != WVRT_C_EXPORT_OK || count != 0 ||
		shared_count(words, SIZE_MAX / sizeof(*words) + 1, NULL, 0, &count) != WVRT_C_EXPORT_INVALID_ARGUMENT ||
		count != 0) { return 7; }
	return 0;
}

/* Check exclusive/exclusive overlap, adjacent writes, and zero-length mutable views. */
static int check_exclusive(void) {
	uint8_t pair[] = { 2, 4 };
	/* An overlapping attempt must not change either byte. */
	if (mark_pair(pair, 2, pair + 1, 1) != WVRT_C_EXPORT_OVERLAP || pair[0] != 2 || pair[1] != 4) { return 8; }
	if (mark_pair(pair, 1, pair + 1, 1) != WVRT_C_EXPORT_OK || pair[0] != 17 || pair[1] != 25) { return 9; }
	if (mark_pair(pair, 0, pair, 1) != WVRT_C_EXPORT_OK || pair[0] != 25 || pair[1] != 25) { return 10; }
	if (mark_pair(NULL, 0, NULL, 0) != WVRT_C_EXPORT_OK) { return 11; }
	return 0;
}

/* Distinguish a completed consumer from any failed check. */
int main(void) {
	int result = check_histogram();
	if (result != 0) { return result; }
	result = check_words();
	if (result != 0) { return result; }
	result = check_exclusive();
	return result == 0 ? 42 : result;
}

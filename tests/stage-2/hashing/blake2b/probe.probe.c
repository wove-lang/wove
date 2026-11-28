// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main wove_fixture_main
#include "blake2b.c"
#undef main

// The graph emitter names these fixture entry functions by the resolved declaration ordinal.
#define check_raw wgf0_check_raw
#define check_fields wgf1_check_fields

// Decode one ASCII hex digit from a manifest field.
static int digit(char byte) {
	if (byte >= '0' && byte <= '9') return byte - '0';
	if (byte >= 'a' && byte <= 'f') return byte - 'a' + 10;
	return -1;
}

// Decode a complete fixture byte span and validate its hexadecimal spelling.
static bool hex_bytes(const char *hex, uint8_t *bytes, size_t length) {
	if (length == 0) return strcmp(hex, "-") == 0;
	if (strlen(hex) != 2 * length) return false;
	// Each pair represents one original fixture byte.
	for (size_t index = 0; index < length; ++index) {
		const int high = digit(hex[2 * index]);
		const int low = digit(hex[2 * index + 1]);
		if (high < 0 || low < 0) return false;
		bytes[index] = (uint8_t)(16 * high + low);
	}
	return true;
}

// Borrow the encoded digest field as ASCII bytes.
static wvrt_slice text(const char *value) {
	return (wvrt_slice){value, (int32_t)strlen(value), sizeof(uint8_t)};
}

// Forward fixture data and return the first failed Wove assertion.
int main(int argc, char **argv) {
	const bool raw = argc == 8 && strcmp(argv[1], "raw") == 0;
	const bool fields = argc == 6 && strcmp(argv[1], "fields") == 0;
	// Require a complete row before accessing its fields.
	if (!raw && !fields) {
		fprintf(stderr, "Expected a raw or fields fixture\n");
		return EXIT_FAILURE;
	}
	const size_t length = strlen(argv[2]) / 2;
	char *end = NULL;
	const long split = strtol(argv[3], &end, 10);
	// The Wove fixture uses signed slice indices.
	if (length > INT32_MAX || end == argv[3] || *end != '\0' || split < 0 || (size_t)split > length) {
		fprintf(stderr, "Invalid fixture input split\n");
		return EXIT_FAILURE;
	}
	uint8_t *bytes = malloc(length + 1);
	uint8_t early[32];
	uint8_t final[32];
	int result = EXIT_FAILURE;
	// Decode only the manifest bytes. The Wove fixture performs all digest checks.
	if (bytes != NULL && hex_bytes(argv[2], bytes, length) && hex_bytes(argv[4], early, sizeof(early))) {
		const wvrt_slice input = {bytes, (int32_t)length, sizeof(uint8_t)};
		const wvrt_slice expected = {early, sizeof(early), sizeof(uint8_t)};
		// A fields case needs one digest. A raw case also includes its prefix digest.
		if (fields) {
			result = check_fields(input, (int32_t)split, expected, text(argv[5]));
		} else if (hex_bytes(argv[6], final, sizeof(final))) {
			result = check_raw(input, (int32_t)split, expected, text(argv[5]),
				(wvrt_slice){final, sizeof(final), sizeof(uint8_t)}, text(argv[7]));
		}
	}
	free(bytes);
	return result;
}

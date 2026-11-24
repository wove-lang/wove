// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main wove_fixture_main
#include "command.c"
#undef main

typedef wv_seed_record_0 ArgSpan;

// Match the seed's first Wove function to the fixture entry.
#define check_invocation wv_seed_fn_0

// Read the supplied JSONC exactly as stored.
static wvrt_slice read_config(const char *path) {
	FILE *file = fopen(path, "rb");
	// Size the input before allocating its byte arena.
	if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	const long size = ftell(file);
	// The Wove views use 32-bit signed byte offsets.
	if (size < 0 || size > INT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Could not size %s\n", path);
		exit(EXIT_FAILURE);
	}
	uint8_t *bytes = malloc((size_t)size + 1);
	// Keep a non-null base even for an empty supplied file.
	if (bytes == NULL || fread(bytes, 1, (size_t)size, file) != (size_t)size || fclose(file) != 0) {
		fprintf(stderr, "Could not read %s\n", path);
		exit(EXIT_FAILURE);
	}
	return (wvrt_slice){bytes, (int32_t)size, sizeof(uint8_t)};
}

// Pack the case's user arguments into the resolver's two borrowed views.
int main(int argc, char **argv) {
	// Fixture arguments specify the source, assertion group, and expected diagnostic.
	if (argc < 8) {
		fprintf(stderr, "Expected command fixture parameters\n");
		return EXIT_FAILURE;
	}
	const int count = argc - 8;
	size_t size = 0;
	// Compute the byte capacity before allocating and copying.
	for (int i = 8; i < argc; ++i) {
		const size_t length = strlen(argv[i]);
		// Prevent a wrapped offset in the test arena.
		if (length > (size_t)INT32_MAX - size) {
			return EXIT_FAILURE;
		}
		size += length;
	}
	ArgSpan *spans = calloc((size_t)count + 1, sizeof *spans);
	uint8_t *bytes = malloc(size + 1);
	// Both views have valid bases even when the command is missing.
	if (spans == NULL || bytes == NULL) {
		free(spans);
		free(bytes);
		return EXIT_FAILURE;
	}
	size_t at = 0;
	// Preserve empty arguments and pack only the bytes inside each value.
	for (int i = 0; i < count; ++i) {
		const char *value = argv[i + 8];
		const size_t length = strlen(value);
		memcpy(bytes + at, value, length);
		spans[i] = (ArgSpan){(int32_t)at, (int32_t)(at + length)};
		at += length;
	}
	const bool has_config = strcmp(argv[1], "-") != 0;
	const wvrt_slice config = has_config ? read_config(argv[1]) : (wvrt_slice){NULL, 0, sizeof(uint8_t)};
	const int result = check_invocation(
		(wvrt_slice){bytes, (int32_t)size, sizeof(uint8_t)},
		(wvrt_slice){spans, count, sizeof *spans}, config, has_config, atoi(argv[2]),
		(uint32_t)atoi(argv[3]), (uint32_t)atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]));
	free((void *)config.wvrt_items);
	free(spans);
	free(bytes);
	// Include the command and Wove assertion number on failure.
	if (result != 0) {
		fprintf(stderr, "%s using %s: command fixture check %d failed\n", count > 0 ? argv[8] : "(empty)", argv[1], result);
	}
	return result;
}

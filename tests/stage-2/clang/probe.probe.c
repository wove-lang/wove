// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main wove_fixture_main
#include "probe.c"
#undef main

// The seed numbers the fixture entry functions in source order.
#define emit_probe wv_seed_fn_0
#define inspect wv_seed_fn_1

// Borrow exactly the payload bytes of one fixture argument.
static wvrt_slice text(const char *value) {
	return (wvrt_slice){value, (int32_t)strlen(value), sizeof(uint8_t)};
}

// Supply template and captured documents without filesystem calls in Wove.
static wvrt_slice read_input(const char *path) {
	FILE *file = fopen(path, "rb");
	// Size the fixture before allocating its arena.
	if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	const long length = ftell(file);
	// The Wove reader accepts a signed 32-bit byte count.
	if (length < 0 || length > INT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Could not size %s\n", path);
		exit(EXIT_FAILURE);
	}
	uint8_t *bytes = malloc((size_t)length + 1);
	// Preserve all source bytes, including spaces and adjacent delimiters.
	if (bytes == NULL || fread(bytes, 1, (size_t)length, file) != (size_t)length || fclose(file) != 0) {
		fprintf(stderr, "Could not read %s\n", path);
		exit(EXIT_FAILURE);
	}
	return (wvrt_slice){bytes, (int32_t)length, sizeof(uint8_t)};
}

// Query capacity, exercise the short-buffer path, and emit exact-sized C source.
static int write_source(wvrt_slice input, wvrt_slice header, wvrt_slice extra) {
	uint8_t empty = 0xa5;
	const wv_seed_record_1 size = emit_probe(input, header, extra, (wvrt_mut_slice){&empty, 0, 1});
	// Invalid names or templates must leave the destination untouched.
	if (size.wv_field_0 != 4) {
		return empty == 0xa5 && size.wv_field_1 == 0 ? (int)size.wv_field_0 : 99;
	}
	const int32_t required = size.wv_field_2;
	// A successful sizing pass must require at least the probe header.
	if (required < 1) {
		return EXIT_FAILURE;
	}
	uint8_t *bytes = malloc((size_t)required + 2);
	// Guard both sides of the exact slice.
	if (bytes == NULL) {
		return EXIT_FAILURE;
	}
	memset(bytes, 0xa5, (size_t)required + 2);
	const wv_seed_record_1 short_view = emit_probe(input, header, extra, (wvrt_mut_slice){bytes + 1, required - 1, 1});
	// A short view reports the exact size without even a partial header.
	if (short_view.wv_field_0 != 4 || short_view.wv_field_1 != 0 || short_view.wv_field_2 != required) {
		free(bytes);
		return EXIT_FAILURE;
	}
	// Every byte is still the guard value after the failed write.
	for (int32_t at = 0; at < required; ++at) {
		// Detect a partial write anywhere in the short view.
		if (bytes[at + 1] != 0xa5) {
			free(bytes);
			return EXIT_FAILURE;
		}
	}
	const wv_seed_record_1 rendered = emit_probe(input, header, extra, (wvrt_mut_slice){bytes + 1, required, 1});
	const bool complete = rendered.wv_field_0 == 0 && rendered.wv_field_1 == required &&
		rendered.wv_field_2 == required && bytes[0] == 0xa5 && bytes[(size_t)required + 1] == 0xa5;
	const int result = complete && fwrite(bytes + 1, 1, (size_t)required, stdout) == (size_t)required ? EXIT_SUCCESS : EXIT_FAILURE;
	free(bytes);
	return result;
}

// Report raw coordinates and the selected probe value for the standalone fixture runner.
int main(int argc, char **argv) {
	// The source mode takes the template, header spelling and optional extra C.
	if (argc == 5 && strcmp(argv[1], "source") == 0) {
		const wvrt_slice input = read_input(argv[2]);
		const int result = write_source(input, text(argv[3]), text(argv[4]));
		free((void *)input.wvrt_items);
		return result;
	}
	// The inspect mode takes one document offset and the names to look up.
	if (argc == 7 && strcmp(argv[1], "inspect") == 0) {
		const wvrt_slice input = read_input(argv[2]);
		const wv_seed_record_0 result = inspect(input, atoi(argv[3]), text(argv[4]), text(argv[5]), text(argv[6]));
		printf("%" PRIu32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32
			"\t%" PRIu32 "\t%" PRId32 "\t%" PRId32 "\t%" PRIu32 "\n",
			result.wv_field_0, result.wv_field_1, result.wv_field_2, result.wv_field_3,
			result.wv_field_4, result.wv_field_5, result.wv_field_6, result.wv_field_7,
			result.wv_field_8, result.wv_field_9);
		free((void *)input.wvrt_items);
		return EXIT_SUCCESS;
	}
	fprintf(stderr, "Expected source or inspect fixture arguments\n");
	return EXIT_FAILURE;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main wove_fixture_main
#include "probe.c"
#undef main

// The seed numbers the fixture entry functions in source order.
#define check_source wv_seed_fn_0
#define check_ast wv_seed_fn_1

// Borrow exactly the payload bytes of one fixture argument.
static wvrt_slice text(const char *value) {
	return (wvrt_slice){value, strcmp(value, "-") == 0 ? 0 : (int32_t)strlen(value), sizeof(uint8_t)};
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

// Pass a source case to Wove and write the checked C bytes for the shell compiler.
static int run_source(char **argv) {
	const wvrt_slice input = read_input(argv[2]);
	const wvrt_slice extra = strcmp(argv[4], "-") == 0 ? text("-") : read_input(argv[4]);
	const wv_seed_record_1 expected = {
		atoi(argv[6]), atoi(argv[7]), atoi(argv[8]), (uint32_t)strtoul(argv[9], NULL, 10)
	};
	const int32_t size = expected.wv_field_2;
	const int32_t capacity = size > 0 ? size : 1;
	uint8_t *output = malloc((size_t)capacity);
	int result = EXIT_FAILURE;
	// The Wove fixture owns the behavior checks for this view.
	if (output != NULL) {
		result = check_source(input, text(argv[3]), extra, text(argv[5]), expected,
			(wvrt_mut_slice){output, capacity, sizeof(uint8_t)});
		// Emit only a successfully checked source fixture.
		if (result == 0 && size > 0 && fwrite(output, 1, (size_t)size, stdout) != (size_t)size) {
			result = EXIT_FAILURE;
		}
	}
	free(output);
	// Only a file-backed extra owns storage.
	if (strcmp(argv[4], "-") != 0) {
		free((void *)extra.wvrt_items);
	}
	free((void *)input.wvrt_items);
	return result;
}

// Pass the capture and the manifest's expected coordinates to the Wove assertions.
static int run_index(int argc, char **argv) {
	const wvrt_slice input = read_input(argv[2]);
	const wv_seed_record_0 expected = {
		(uint32_t)strtoul(argv[7], NULL, 10), atoi(argv[8]), atoi(argv[9]), atoi(argv[10]),
		atoi(argv[11]), atoi(argv[12]), (uint32_t)strtoul(argv[13], NULL, 10),
		atoi(argv[14]), atoi(argv[15]), (uint32_t)strtoul(argv[16], NULL, 10)
	};
	const wvrt_slice expected_text = text(argc == 18 ? argv[17] : "-");
	const int result = check_ast(input, atoi(argv[3]), text(argv[4]), text(argv[5]), text(argv[6]), expected, expected_text);
	free((void *)input.wvrt_items);
	return result;
}

// Dispatch transport modes using the case arguments supplied by the shell.
int main(int argc, char **argv) {
	// Guard each mode's argument count before reading its fields.
	if (argc == 10 && strcmp(argv[1], "source") == 0) {
		return run_source(argv);
	}
	// Index cases may give the original probe text alongside span and word expectations.
	if ((argc == 17 || argc == 18) && strcmp(argv[1], "index") == 0) {
		return run_index(argc, argv);
	}
	fprintf(stderr, "Expected source or index fixture arguments\n");
	return EXIT_FAILURE;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main wove_fixture_main
#include "project.c"
#undef main

// Match the first Wove function in the standalone project fixture.
#define check_project wv_seed_fn_0

// Borrow a string without its terminating NUL.
static wvrt_slice text(const char *bytes) {
	return (wvrt_slice){(const uint8_t *)bytes, (int32_t)strlen(bytes), sizeof(uint8_t)};
}

// Pass actual file contents to the Wove fixture, alongside the requested profile.
static void check(const char *label, wvrt_slice source, const char *profile, int scenario,
	uint32_t error, int32_t start, int32_t end) {
	const int result = check_project(source, text(profile), scenario, error, start, end);
	// Identify the fixture when a Wove assertion fails.
	if (result != 0) {
		fprintf(stderr, "%s (%s): project fixture check %d failed\n", label, profile, result);
		exit(EXIT_FAILURE);
	}
}

typedef struct {
	char *bytes;
	wvrt_slice source;
} ConfigFile;

// Load the checked-in JSONC without introducing filesystem access into the schema.
static ConfigFile read_config(const char *path) {
	FILE *file = fopen(path, "rb");
	// Size the fixture before allocating bytes for its source view.
	if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	const long size = ftell(file);
	// The generated fixture accepts a signed 32-bit length.
	if (size < 0 || size > INT32_MAX || fseek(file, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Could not size %s\n", path);
		exit(EXIT_FAILURE);
	}
	char *bytes = malloc((size_t)size + 1);
	// Pass exactly the bytes on disk to the JSONC reader.
	if (bytes == NULL || fread(bytes, 1, (size_t)size, file) != (size_t)size || fclose(file) != 0) {
		fprintf(stderr, "Could not read %s\n", path);
		exit(EXIT_FAILURE);
	}
	return (ConfigFile){bytes, {(const uint8_t *)bytes, (int32_t)size, sizeof(uint8_t)}};
}

// Read one config file and check it against the case's expected result.
int main(int argc, char **argv) {
	// The manifest supplies a file, profile, assertion group, error and byte span.
	if (argc != 7) {
		fprintf(stderr, "Expected a config path, profile, assertions, error code and span\n");
		return EXIT_FAILURE;
	}
	const ConfigFile config = read_config(argv[1]);
	check(argv[1], config.source, argv[2], atoi(argv[3]), (uint32_t)atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
	free(config.bytes);
	return EXIT_SUCCESS;
}

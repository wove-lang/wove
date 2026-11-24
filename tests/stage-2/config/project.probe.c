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

typedef struct {
	const char *label;
	const char *source;
	const char *profile;
	const char *span;
	uint32_t error;
} InvalidProject;

// Run the checked-in file, an independent sizing fixture, and exact diagnostic cases.
int main(int argc, char **argv) {
	// Make supplies the repository config and the nested sizing fixture.
	if (argc != 3) {
		fprintf(stderr, "Expected the checked-in and sizing JSONC paths\n");
		return EXIT_FAILURE;
	}
	const ConfigFile project = read_config(argv[1]);
	check(argv[1], project.source, "", 1, 0, -1, -1);
	check(argv[1], project.source, "default", 1, 0, -1, -1);
	check(argv[1], project.source, "bootstrap", 2, 0, -1, -1);
	check(argv[1], project.source, "tests", 3, 0, -1, -1);
	free(project.bytes);

	const ConfigFile sizing = read_config(argv[2]);
	check(argv[2], sizing.source, "wide", 4, 0, -1, -1);
	check(argv[2], sizing.source, "auto", 5, 0, -1, -1);
	free(sizing.bytes);
	check("built-in defaults", text("{}"), "", 6, 0, -1, -1);
	check("explicit built-in default", text("{\"profiles\": [/* empty */],}"), "default", 6, 0, -1, -1);
	check("trailing array comma before comment", text("{\"profiles\": [{}, /* trailing */],}"), "", 6, 0, -1, -1);

	const InvalidProject invalid[] = {
		{"empty profile name", "{\"profiles\":[{\"name\":\"\"},],}", "", "\"\"", 17},
		{"escaped line break in name", "{\"profiles\":[{\"name\":\"bad\\u000aname\"}]}", "", "\"bad\\u000aname\"", 17},
		{"nonstring name", "{\"profiles\":[{\"name\":null}]}", "", "null", 16},
		{"duplicate decoded name", "{\"profiles\":[{\"name\":\"p\"},{\"name\":\"\\u0070\"}]}", "", "\"\\u0070\"", 18},
		{"duplicate implicit default", "{\"profiles\":[{},{\"name\":\"default\"}]}", "", "\"default\"", 18},
		{"unknown selected profile", "{\"profiles\":[{\"name\":\"test\"}]}", "missing", "[{\"name\":\"test\"}]", 19},
		{"unknown profile without a list", "{}", "missing", "{}", 19},
		{"bad width in unselected profile", "{\"profiles\":[{}, {\"name\":\"bad\",\"fmt\":{\"lineWidth\":-1,},},]}", "", "-1", 16},
		{"noninteger parallelJobs", "{\"profiles\":[{\"parallelJobs\":1.5}]}", "", "1.5", 16},
		{"worker count overflow", "{\"profiles\":[{\"parallelJobs\":4294967296}]}", "", "4294967296", 16},
		{"unsupported LTO mode", "{\"profiles\":[{\"build\":{\"release\":{\"lto\":\"wide\"}}}]}", "", "\"wide\"", 16},
		{"invalid list element", "{\"profiles\":[{\"entry\":{\"bin\":[1,],},},]}", "", "1", 16},
		{"unknown nested field", "{\"profiles\":[{\"fmt\":{\"width\":80}}]}", "", "\"width\"", 14},
		{"duplicate decoded field", "{\"profiles\":[{\"cc\":{\"flat\":false,\"fl\\u0061t\":true}}]}", "", "\"fl\\u0061t\"", 15},
		{"leading array comma", "{\"profiles\":[,]}", "", ",", 2},
		{"double array comma", "{\"profiles\":[{},,]}", "", ",", 2},
		{"leading object comma", "{,}", "", ",", 7},
		{"double object comma", "{\"profiles\":[],,}", "", ",", 7},
	};
	// Each expected span is a unique literal, or the second of two consecutive commas.
	for (size_t index = 0; index < sizeof(invalid) / sizeof(invalid[0]); ++index) {
		const InvalidProject item = invalid[index];
		const char *first = strstr(item.source, item.span);
		// A miss here means the fixture's expected span is misspelled.
		if (first == NULL) {
			fprintf(stderr, "%s: expected span is absent\n", item.label);
			return EXIT_FAILURE;
		}
		// The first comma in a doubled pair is a valid separator.
		if (first[0] == ',' && first[1] == ',') {
			++first;
		}
		const int32_t start = (int32_t)(first - item.source);
		check(item.label, text(item.source), item.profile, 0, item.error, start, start + (int32_t)strlen(item.span));
	}
	return EXIT_SUCCESS;
}

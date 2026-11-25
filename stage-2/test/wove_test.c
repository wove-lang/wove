// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_test.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "../io/process.h"

// Admit only nonempty decimal indices within the dispatch table.
static int run_selected(const char *text, const wove_test_case *tests, size_t count) {
	if (*text < '0' || *text > '9') { return 2; }
	char *end;
	errno = 0;
	const uintmax_t index = strtoumax(text, &end, 10);
	if (errno != 0 || *end != '\0' || index >= count) { return 2; }
	tests[index].run();
	return 0;
}

// Keep report fields on one row even when a source path contains a separator.
static void write_field(const char *text) {
	// Escape field and record separators, along with the escape character.
	for (; *text != '\0'; ++text) {
		// Each supported escape has the same backslash prefix.
		switch (*text) {
		case '\\': fputs("\\\\", stdout); break;
		case '\t': fputs("\\t", stdout); break;
		case '\r': fputs("\\r", stdout); break;
		case '\n': fputs("\\n", stdout); break;
		default: putchar((unsigned char)*text); break;
		}
	}
}

// Call the common capture API once with a NUL-terminated selection argument.
static int32_t launch_test(const char *program, int32_t length, size_t index) {
	char selection[3 * sizeof(size_t) + 1];
	const int width = snprintf(selection, sizeof(selection), "%zu", index);
	if (width < 0 || (size_t)width >= sizeof(selection)) { return -1; }
	uint8_t capture[5];
	return wove_process_capture((const uint8_t *)program, length, (const uint8_t *)selection,
		(int32_t)width + 1, NULL, 0, capture, sizeof(capture));
}

// Run each test in a new child, using the same numeric selection as direct dispatch.
int wove_test_entry(int argc, char **argv, const wove_test_case *tests, size_t count) {
	if (argc == 2) { return run_selected(argv[1], tests, count); }
	if (argc != 1) { return 2; }
	const size_t length = strlen(argv[0]);
	if (length > INT32_MAX) { return 2; }
	int failed = 0;
	// A trap supplies a nonzero child status and leaves later tests runnable.
	for (size_t index = 0; index < count; ++index) {
		const int32_t status = launch_test(argv[0], (int32_t)length, index);
		fputs(status == 0 ? "pass\t" : "fail\t", stdout);
		write_field(tests[index].file);
		putchar('\t');
		write_field(tests[index].name);
		printf("\t%" PRId32 "\n", status);
		failed |= status != 0;
	}
	return fflush(stdout) == EOF || failed;
}

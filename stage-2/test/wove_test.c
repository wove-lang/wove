// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_test.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#if defined(__APPLE__)
#include <libproc.h>
#include <unistd.h>
#endif

// The native Wove runner is compiled with this entry name.
int wvrt_test_parent(int argc, char **argv);

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

// List escaped labels in the same order as numeric dispatch.
static int list_inventory(const wove_test_case *tests, size_t count) {
	// Each record preserves the two report fields and occupies one physical line.
	for (size_t index = 0; index < count; ++index) {
		write_field(tests[index].file);
		putchar('\t');
		write_field(tests[index].name);
		putchar('\n');
	}
	return fflush(stdout) == EOF || ferror(stdout);
}

// Pass the running image path as the Wove runner's sole user argument.
static int run_parent(char *display_name) {
	char *program = NULL;
	// Resolve the executable independently of the invocation's display name.
#if defined(__APPLE__)
	char path[PROC_PIDPATHINFO_MAXSIZE];
	if (proc_pidpath(getpid(), path, sizeof(path)) > 0) { program = path; }
#elif defined(__linux__)
	// The child still refers to this image when spawn opens the procfs link.
	char path[] = "/proc/self/exe";
	program = path;
#endif
	// Without a path the inventory cannot launch its child processes.
	if (program == NULL) {
		fputs("could not locate the running Wove test executable\n", stderr);
		return 1;
	}
	char *arguments[] = {display_name, program, NULL};
	return wvrt_test_parent(2, arguments);
}

// List the inventory, run one numeric selection, or enter the Wove parent.
int wove_test_entry(int argc, char **argv, const wove_test_case *tests, size_t count) {
	// The inventory query and numeric selection both run directly in this process.
	if (argc == 2) {
		if (strcmp(argv[1], "--list") == 0) { return list_inventory(tests, count); }
		return run_selected(argv[1], tests, count);
	}
	if (argc != 1) { return 2; }
	if (count == 0) { return 0; }
	return run_parent(argv[0]);
}

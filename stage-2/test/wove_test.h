// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_H
#define WOVE_TEST_H

#include <stddef.h>

typedef struct {
	const char *file;
	const char *name;
	void (*run)(void);
} wove_test_case;

// List the inventory, dispatch one numeric selection, or enter the Wove runner.
int wove_test_entry(int argc, char **argv, const wove_test_case *tests, size_t count);

// Open main and its local table, keeping its names out of the file's C namespace.
#define WVRT_TEST_BEGIN \
	int main(int argc, char **argv) { \
		static const wove_test_case wvrt_tests[] = {

// A sentinel keeps the empty inventory valid C and stays outside the dispatch count.
#define WVRT_TEST_END \
		{NULL, NULL, NULL}}; \
		return wove_test_entry(argc, argv, wvrt_tests, sizeof(wvrt_tests) / sizeof(wvrt_tests[0]) - 1); \
	}

#endif

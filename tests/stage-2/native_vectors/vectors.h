// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_VECTORS_H
#define WOVE_TEST_VECTORS_H
#include <errno.h>
#include <stdint.h>
#include <string.h>

// Check exact pointer terminators and mutate only the temporary characters.
static inline int32_t test_vectors(char *const *rows, char *const *empty) {
	if (rows == NULL || empty == NULL || empty[0] != NULL) { return 1; }
	// Three rows must arrive intact, with one distinct empty row.
	if (rows[0] == NULL || rows[1] == NULL || rows[2] == NULL || rows[3] != NULL) { return 2; }
	// Spaced bytes and empty content retain their original boundaries.
	if (strcmp(rows[0], "first row") != 0 || rows[1][0] != '\0' || strcmp(rows[2], "last") != 0) { return 3; }
	rows[0][0] = 'X';
	errno = EDOM;
	return 42;
}

// Return a borrowed alias of a vector row and write back through the mutable text.
static inline char *test_vector_result(char *const *rows, char *text) {
	text[0] = 'Z';
	return rows[0];
}

enum { WOVE_TEST_FIXED_LEFT = 40, WOVE_TEST_FIXED_RIGHT = 2 };

// Check that the detached vector stays between two implicit native operands.
static inline int32_t test_fixed_vector(int32_t left, char *const *rows, int32_t right) {
	// Require both native operand positions, the detached row, and its pointer sentinel.
	if (left != WOVE_TEST_FIXED_LEFT || right != WOVE_TEST_FIXED_RIGHT ||
		rows == NULL || rows[0] == NULL || rows[1] != NULL || strcmp(rows[0], "two words") != 0) {
		return 1;
	}
	rows[0][0] = 'X';
	return left + right;
}

// Observe errno immediately after the call's vector cleanup.
static inline int32_t test_vector_errno(void) {
	return errno == EDOM ? 0 : 1;
}

static char test_live_row[] = "before";
static char test_empty_row[] = "";
static char *test_live_table[] = {test_live_row, test_empty_row, NULL};
char **test_vector_global = test_live_table;

// Change the original bytes, then remove the original native table.
static inline int32_t test_null_vector(void) {
	test_live_row[0] = 'X';
	test_vector_global = NULL;
	return 0;
}

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_SEED_BRIDGE_VECTORS_H
#define WOVE_SEED_BRIDGE_VECTORS_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static char fixture_first[] = "prefix";
static char fixture_empty[] = "";
static char *fixture_table[] = {fixture_first, fixture_empty, NULL};
static char **fixture_environment = fixture_table;

// Mutate the detached argument and the native table to expose incorrect sharing.
static inline int32_t fixture_vectors(char *const *rows, char *const *empty) {
	// Each converted vector has its own null terminator, including an empty vector.
	if (empty[0] != NULL || rows[0] == NULL || rows[1] == NULL || rows[2] != NULL ||
		strcmp(rows[0], "prefix") != 0 || rows[1][0] != '\0') {
		return 1;
	}
	rows[0][0] = 'z';
	fixture_first[0] = 'b';
	return 0;
}

#endif

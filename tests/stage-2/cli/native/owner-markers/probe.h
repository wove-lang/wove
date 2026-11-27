/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_OWNER_MARKER_H
#define WOVE_TEST_OWNER_MARKER_H

#include <stdint.h>
#include <stdlib.h>

typedef struct ProbeAcl { int32_t value; } *ProbeAcl;
typedef struct OtherAcl *OtherAcl;

#define PROBE_REMOVE ((void *)1)
#define PROBE_INTEGER 1

static uintptr_t original_slot;
static int32_t drops;

/* Model a native destructor which accepts the owner as void and returns a status. */
static inline int32_t drop_probe(void *value)
{
	if (!value || value == PROBE_REMOVE) {
		abort();
	}
	++drops;
	free(value);
	return 0;
}

/* Report absence, a marker, or an owned allocation independently of native status. */
static inline int32_t get_probe(int32_t mode, int32_t selector, void *output, int32_t token)
{
	if (selector != 13 || token != 29 || *(ProbeAcl *)output) {
		abort();
	}
	original_slot = (uintptr_t)output;
	if (mode == 0) {
		return -13;
	}
	if (mode == 1 || mode == 4) {
		*(ProbeAcl *)output = PROBE_REMOVE;
		return mode == 1 ? 0 : -5;
	}
	ProbeAcl created = malloc(sizeof(*created));
	if (!created) {
		abort();
	}
	created->value = 42;
	*(ProbeAcl *)output = created;
	return mode == 2 ? 9 : -8;
}

/* Check that the original native handle slot still holds its allocation. */
static inline int32_t read_probe(const void *input)
{
	if ((uintptr_t)input != original_slot || !*(ProbeAcl const *)input) {
		abort();
	}
	return (*(ProbeAcl const *)input)->value;
}

/* Distinguish the direct marker operand from a pointer to a handle slot. */
static inline int32_t mark_probe(const void *marker)
{
	return marker == PROBE_REMOVE ? 17 : -1;
}

/* Report destructor calls after lexical owner scopes finish. */
static inline int32_t drop_count(void)
{
	return drops;
}

/* Provide a deliberately incompatible qualifier for the exact native check. */
static inline int32_t wrong_get(const void *output)
{
	return output ? 0 : -1;
}

/* Deliberately violate the required mutable void-pointer destructor ABI. */
static inline int32_t wrong_drop(const void *value)
{
	return value ? 0 : -1;
}

#endif

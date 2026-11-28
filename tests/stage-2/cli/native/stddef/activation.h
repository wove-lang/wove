/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_STDDEF_ACTIVATION_H
#define WOVE_TEST_STDDEF_ACTIVATION_H

#include <stddef.h>
#include <stdint.h>

struct fixture_layout {
	uint8_t tag;
	uint64_t payload;
};

/* Observe absence or increment the live loan supplied by Wove. */
static inline int32_t fixture_nullable(int32_t *value)
{
	return value == NULL ? 42 : ++*value;
}

/* Compare both offsets against native values for this SDK and target. */
static inline int32_t fixture_offsets(size_t tag, size_t payload)
{
	return tag == offsetof(struct fixture_layout, tag) &&
	    payload == offsetof(struct fixture_layout, payload) ? 42 : 1;
}

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/* Produce a value of the selected header's exact C23 type. */
static inline nullptr_t fixture_nullptr(void)
{
	return nullptr;
}

/* Pass the same native type through both sides of the function ABI. */
static inline nullptr_t fixture_nullptr_copy(nullptr_t value)
{
	return value;
}

/* Inspect the scoped loan of C23 storage when it is present. */
static inline int32_t fixture_nullptr_read(const nullptr_t *value)
{
	if (value == NULL)
		return 7;
	return *value == nullptr ? 42 : 1;
}
#endif

#endif

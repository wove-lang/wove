/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_STATIC_CSTR_H
#define WOVE_TEST_STATIC_CSTR_H

#include <stddef.h>

static const char *fixture_retained;

/* Keep the caller's pointer, including when replacing an existing registration. */
static inline int fixture_register(int severity, const char *text)
{
	if (severity != 91 || (text != NULL && text[0] == '!'))
		return -1;
	if (text == NULL && fixture_retained == NULL)
		return -1;
	fixture_retained = text;
	return 0;
}

/* Inspect the original bytes from a later Wove call. */
static inline int fixture_read(int index)
{
	return fixture_retained != NULL ? (unsigned char)fixture_retained[index] : -1;
}

/* A mutable native formal must fail the exact retained-string prototype check. */
static inline int fixture_wrong(int severity, char *text)
{
	return severity + (text != NULL);
}

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_NATIVE_NULLPTR_H
#define WOVE_TEST_NATIVE_NULLPTR_H

#if defined(INTEGER_ALIAS)
typedef int FixtureNull;
#elif defined(POINTER_ALIAS)
typedef void *FixtureNull;
#elif defined(QUALIFIED_ALIAS)
typedef const __typeof__(nullptr) FixtureNull;
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef __typeof__(nullptr) ProjectNull;
typedef ProjectNull FixtureNull;
#endif

#ifdef TEST_NULLPTR_SIGNATURES

typedef struct {
	FixtureNull item;
} FixtureBox;

/* Produce a native null value through its own C23 result ABI. */
static inline FixtureNull fixture_null(void)
{
	return nullptr;
}

/* Preserve the exact native type on both sides of a call. */
static inline FixtureNull fixture_roundtrip(FixtureNull value)
{
	return value;
}

/* Observe the original C storage through a shared Wove loan. */
static inline int fixture_read(const FixtureNull *storage)
{
	return storage == nullptr || *storage == nullptr;
}

/* Write a native result into the caller's mutable Wove storage. */
static inline int fixture_write(FixtureNull *storage)
{
	*storage = nullptr;
	return 1;
}

/* Use a convertible native parameter with a different declaration type. */
static inline int fixture_wrong_argument(void *value)
{
	return value == nullptr;
}

/* Use a C pointer result that cannot satisfy the exact native null result. */
static inline void *fixture_wrong_result(void)
{
	return nullptr;
}

#endif
#endif

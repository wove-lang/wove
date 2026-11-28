// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_FIXTURE_NULL_VALUE_H
#define WOVE_FIXTURE_NULL_VALUE_H

#include <stddef.h>
#include <stdint.h>

extern int32_t fixture_nonconstant;
extern int32_t fixture_storage;

#if defined(FIXTURE_NULL_C23)
#define PROJECT_NULL nullptr
#elif defined(FIXTURE_NULL_ZERO)
#define PROJECT_NULL 0
#elif defined(FIXTURE_NULL_DEBUG_CONTEXT)
// The retained fixture calls on line 7. Debug staging adds one physical line.
#define PROJECT_NULL (__LINE__ - 7)
#elif defined(FIXTURE_NULL_CONTEXT)
#define PROJECT_NULL __COUNTER__
#elif defined(FIXTURE_NULL_NONZERO)
#define PROJECT_NULL 1
#elif defined(FIXTURE_NULL_NONCONSTANT)
#define PROJECT_NULL fixture_nonconstant
#elif defined(FIXTURE_NULL_POINTER)
#define PROJECT_NULL ((void *)&fixture_storage)
#elif defined(FIXTURE_NULL_NESTED)
#define PROJECT_NULL ((int32_t *)((void *)0))
#elif defined(FIXTURE_NULL_FUNCTION)
enum { PROJECT_NULL = 0 };
#define PROJECT_NULL(value) (value)
#elif !defined(FIXTURE_NULL_MISSING)
#define PROJECT_NULL ((void *)0)
#endif

// Observe absence and verify that a present argument uses the caller's storage.
static inline int32_t fixture_nullable(int32_t *value) {
	if (value == NULL) { return 42; }
	return ++*value;
}

// Exercise a call-scoped C string, accepting the macro without a scratch copy.
static inline int32_t fixture_nullable_string(const char *value) {
	return value == NULL ? 42 : 1;
}

// Exercise a native void pointer that needs no conversion from the system NULL.
static inline int32_t fixture_nullable_void(void *value) {
	return value == NULL ? 42 : 1;
}

// This native function requires a present argument in Wove.
static inline int32_t fixture_plain(int32_t *value) {
	return *value;
}

// The Wove declaration accepts only the literal null.
static inline int32_t fixture_literal_only(void *value) {
	return value == NULL ? 42 : 1;
}

// The checked bridge exposes the open variadic tail.
static inline int32_t fixture_variadic(int32_t value, ...) {
	return value;
}

#endif

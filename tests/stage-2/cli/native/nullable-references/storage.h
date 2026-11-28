// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NULLABLE_STORAGE_H
#define WOVE_TEST_NULLABLE_STORAGE_H

#include <stdint.h>

typedef int32_t *fixture_slot;

static fixture_slot *initialized;
static int32_t payload = 73;
static int32_t destroyed;

// Remember the original native address of this pointer-backed storage object.
static inline int32_t fixture_init(fixture_slot *storage)
{
    *storage = &payload;
    initialized = storage;
    return 0;
}

// Only destruction of the original active storage counts as cleanup.
static inline int32_t fixture_destroy(fixture_slot *storage)
{
    if (storage != initialized || *storage != &payload)
        return -1;
    initialized = 0;
    ++destroyed;
    return 0;
}

#ifdef WOVE_FIXTURE_MUTABLE_STORAGE
#define WOVE_FIXTURE_STORAGE_CONST
#else
#define WOVE_FIXTURE_STORAGE_CONST const
#endif

// Mark which nullable arguments were present and check their native address.
static inline int32_t fixture_probe(WOVE_FIXTURE_STORAGE_CONST fixture_slot *left, const fixture_slot *right)
{
    if ((left && (left != initialized || *left != &payload)) ||
        (right && (right != initialized || *right != &payload)))
        return -1;
    return (left ? 1 : 0) + (right ? 2 : 0);
}

// Observe cleanup after the Wove owner has gone out of scope.
static inline int32_t fixture_drops(void)
{
    return destroyed;
}

#endif

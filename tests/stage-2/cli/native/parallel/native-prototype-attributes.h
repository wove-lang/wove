// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_PROTOTYPE_ATTRIBUTES_H
#define WOVE_PARALLEL_NATIVE_PROTOTYPE_ATTRIBUTES_H

typedef struct fixture_allocation *fixture_block;

fixture_block fixture_allocate_aligned(unsigned long alignment, unsigned long count, unsigned long bytes)
    __attribute__((alloc_align(1), alloc_size(2, 3)));
fixture_block fixture_allocate_aligned(unsigned long alignment, unsigned long count, unsigned long bytes);
fixture_block fixture_allocate(unsigned long bytes) __attribute__((alloc_size(1)));
fixture_block fixture_allocate(unsigned long bytes);
int fixture_release(fixture_block block);
int fixture_live(void);

[[nodiscard("keep the result")]] int fixture_score(int value) __attribute__((const));
int fixture_score(int value);
int fixture_legacy(int value) __attribute__((deprecated("old bridge", "fixture_score")));
int fixture_legacy(int value);

#endif

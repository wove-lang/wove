// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_STORAGE_H
#define WOVE_PARALLEL_NATIVE_STORAGE_H

typedef struct {
    _Alignas(64) int value;
} fixture_storage;

int fixture_init(fixture_storage *storage, int value);
int fixture_read(const fixture_storage *storage);
int fixture_increment(fixture_storage *storage);
int fixture_destroy(fixture_storage *storage) __attribute__((nothrow, nonnull(1)));
int fixture_other_destroy(fixture_storage *storage);
int fixture_current(void);
int fixture_drops(void);

#endif

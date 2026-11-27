// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_RECORD_REFS_H
#define WOVE_PARALLEL_NATIVE_RECORD_REFS_H

typedef struct {
    int value;
    int cookie;
} fixture_info;

#if defined(__APPLE__)
int fixture_capture(const char *path, fixture_info *info) __asm__("_fixture_native_capture")
    __attribute__((nothrow, nonnull(1, 2)));
#else
int fixture_capture(const char *path, fixture_info *info) __asm__("fixture_native_capture")
    __attribute__((nothrow, nonnull(1, 2)));
#endif
int fixture_other_capture(const char *path, fixture_info *info);
int fixture_update(int value, fixture_info *info);
int fixture_read(const fixture_info *info);

#endif

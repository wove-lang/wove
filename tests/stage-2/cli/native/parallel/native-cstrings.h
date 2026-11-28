// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_CSTRINGS_H
#define WOVE_PARALLEL_NATIVE_CSTRINGS_H

#if defined(__APPLE__)
typedef unsigned short fixture_path_mode;
int fixture_mkdir(const char *path, fixture_path_mode mode) __asm__("_fixture_native_mkdir")
    __attribute__((nothrow, nonnull(1)));
#else
typedef unsigned int fixture_path_mode;
int fixture_mkdir(const char *path, fixture_path_mode mode) __asm__("fixture_native_mkdir")
    __attribute__((nothrow, nonnull(1)));
#endif
int fixture_unlink(const char *path) __attribute__((nothrow, nonnull(1)));
int fixture_rename(const char *from, const char *to) __attribute__((nothrow, nonnull(1)));
int fixture_alternate_rename(const char *from, const char *to);

#endif

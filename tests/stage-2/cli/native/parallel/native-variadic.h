// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_VARIADIC_H
#define WOVE_PARALLEL_NATIVE_VARIADIC_H

#if defined(__APPLE__)
typedef unsigned short fixture_mode;
int fixture_open(const char *path, int flags, ...) __asm__("_fixture_native_open")
    __attribute__((nothrow, nonnull(1)));
#else
typedef unsigned int fixture_mode;
int fixture_open(const char *path, int flags, ...) __asm__("fixture_native_open")
    __attribute__((nothrow, nonnull(1)));
#endif
int fixture_other_open(const char *path, int flags, ...);

#endif

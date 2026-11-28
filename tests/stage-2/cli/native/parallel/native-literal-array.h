// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_LITERAL_ARRAY_H
#define WOVE_PARALLEL_NATIVE_LITERAL_ARRAY_H

typedef int fixture_descriptor;

#if defined(__APPLE__)
int fixture_fill(fixture_descriptor descriptors[2], int flags) __asm__("_fixture_native_fill")
    __attribute__((nothrow, nonnull(1)));
#else
int fixture_fill(fixture_descriptor descriptors[2], int flags) __asm__("fixture_native_fill")
    __attribute__((nothrow, nonnull(1)));
#endif
int fixture_alternate_fill(fixture_descriptor descriptors[2], int flags);

#endif

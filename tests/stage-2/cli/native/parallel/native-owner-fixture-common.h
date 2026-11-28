// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_OWNER_FIXTURE_COMMON_H
#define WOVE_PARALLEL_NATIVE_OWNER_FIXTURE_COMMON_H

typedef struct fixture_stream *fixture_handle;

int fixture_close(fixture_handle handle)
#if defined(__APPLE__)
    __asm__("_fixture_native_close")
#else
    __asm__("fixture_native_close")
#endif
    __attribute__((nothrow, nonnull(1)));

int fixture_alternate_close(fixture_handle handle);
int fixture_stream_counts(void);

#endif

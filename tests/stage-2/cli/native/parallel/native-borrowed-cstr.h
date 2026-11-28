// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_BORROWED_CSTR_H
#define WOVE_PARALLEL_NATIVE_BORROWED_CSTR_H

typedef const char *fixture_text;

#if defined(__APPLE__)
fixture_text fixture_lookup(const char *name) __asm__("_fixture_native_lookup")
    __attribute__((nothrow, nonnull(1)));
#else
fixture_text fixture_lookup(const char *name) __asm__("fixture_native_lookup")
    __attribute__((nothrow, nonnull(1)));
#endif
fixture_text fixture_alternate_lookup(const char *name);
int fixture_replace(void);

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_MUTABLE_CSTR_H
#define WOVE_PARALLEL_NATIVE_MUTABLE_CSTR_H

typedef char fixture_char;
typedef fixture_char *fixture_path;

#if defined(__APPLE__)
fixture_path fixture_edit(fixture_path path) __asm__("_fixture_native_edit")
    __attribute__((nothrow, nonnull(1)));
#else
fixture_path fixture_edit(fixture_path path) __asm__("fixture_native_edit")
    __attribute__((nothrow, nonnull(1)));
#endif
fixture_path fixture_alternate_edit(fixture_path path);

#endif

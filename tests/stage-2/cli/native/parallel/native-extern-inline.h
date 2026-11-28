// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_EXTERN_INLINE_H
#define WOVE_PARALLEL_NATIVE_EXTERN_INLINE_H

extern int fixture_target(int value);
extern int fixture_alternate(int value);

extern inline __attribute__((gnu_inline)) int fixture_forward(int value) {
#if defined(WOVE_INLINE_CHANGE_BODY) && __has_include("unit-0000000.c")
    return fixture_alternate(value);
#else
    return fixture_target(value);
#endif
}

#endif

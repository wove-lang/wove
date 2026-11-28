// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_INLINE_CONVERSION_H
#define WOVE_PARALLEL_NATIVE_INLINE_CONVERSION_H

extern const char *fixture_negative_text;
extern const char *fixture_long_text;
extern void fixture_drop_text(const char *text);
extern long fixture_strtol(const char *text, char **end, int base) __attribute__((nothrow, nonnull(1)));
extern int fixture_as_int(const char *text) __attribute__((pure, nothrow, nonnull(1)));
extern long fixture_as_long(const char *text) __attribute__((pure, nothrow, nonnull(1)));

#if !defined(WOVE_INLINE_CONVERSION_IMPLEMENTATION)
extern inline __attribute__((gnu_inline)) int fixture_as_int(const char *text) {
    return (int)fixture_strtol(text, (char **)((void *)0), 10);
}

extern inline __attribute__((gnu_inline)) long fixture_as_long(const char *text) {
    /* The numeric character escape also has integer value 10. */
    return fixture_strtol(text, (char **)((void *)0), '\012');
}
#endif

#endif

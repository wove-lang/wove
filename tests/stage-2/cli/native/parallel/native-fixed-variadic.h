// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_FIXED_VARIADIC_H
#define WOVE_PARALLEL_NATIVE_FIXED_VARIADIC_H

#define FIXTURE_SET 21
enum { FIXTURE_DUP = 27 };

#if defined(__APPLE__)
int fixture_control(int descriptor, int command, ...) __asm__("_fixture_native_control") __attribute__((nothrow));
#else
int fixture_control(int descriptor, int command, ...) __asm__("fixture_native_control") __attribute__((nothrow));
#endif

#endif

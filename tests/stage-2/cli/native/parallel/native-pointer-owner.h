// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_POINTER_OWNER_H
#define WOVE_PARALLEL_NATIVE_POINTER_OWNER_H

#include "native-owner-fixture-common.h"

fixture_handle fixture_open(const char *restrict path, const char *restrict mode)
#if defined(__APPLE__)
    __asm__("_fixture_native_open")
    __attribute__((availability(macos, introduced = 10.5)))
#else
    __asm__("fixture_native_open")
#endif
    __attribute__((nothrow, nonnull(1, 2)));

fixture_handle fixture_alternate_open(const char *path, const char *mode);

#endif

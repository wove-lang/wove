// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_SCALAR_OWNER_H
#define WOVE_PARALLEL_NATIVE_SCALAR_OWNER_H

#include "native-owner-fixture-common.h"

typedef int fixture_descriptor;

fixture_handle fixture_adopt(fixture_descriptor descriptor)
#if defined(__APPLE__)
    __asm__("_fixture_native_adopt")
#else
    __asm__("fixture_native_adopt")
#endif
    __attribute__((nothrow));

fixture_handle fixture_alternate_adopt(fixture_descriptor descriptor);

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_OWNER_CONSUME_H
#define WOVE_PARALLEL_NATIVE_OWNER_CONSUME_H

#if defined(WOVE_AST_GROWTH)
#include "ast-growth.h"
#endif

typedef struct fixture_resource *fixture_resource_handle;

fixture_resource_handle fixture_open_resource(int status);

#if defined(WOVE_FIXTURE_CHANGE_PROTOTYPE) && __has_include("unit-0000000.c")
long
#else
int
#endif
fixture_close_resource(fixture_resource_handle resource)
#if defined(WOVE_FIXTURE_CHANGE_LINK) && __has_include("unit-0000000.c")
#if defined(__APPLE__)
    __asm__("_fixture_other_close")
#else
    __asm__("fixture_other_close")
#endif
#elif defined(__APPLE__)
    __asm__("_fixture_native_close")
#else
    __asm__("fixture_native_close")
#endif
    __attribute__((nothrow))
#if defined(WOVE_FIXTURE_CHANGE_ATTR) && __has_include("unit-0000000.c")
    __attribute__((nonnull(1)))
#endif
    ;

int fixture_resource_drops(void);

#endif

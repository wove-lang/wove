// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_NULLABLE_INTEGER_H
#define WOVE_PARALLEL_NATIVE_NULLABLE_INTEGER_H

typedef int fixture_child;
#ifdef WOVE_FIXTURE_WIDE_STATUS
typedef long fixture_status;
#else
typedef int fixture_status;
#endif

fixture_child fixture_wait(fixture_child child, fixture_status *status, int options)
#if defined(__APPLE__)
    __asm__("_fixture_native_wait")
    __attribute__((availability(macos, introduced = 10.5)))
#else
    __asm__("fixture_native_wait")
#endif
    __attribute__((nothrow));
fixture_child fixture_alternate_wait(fixture_child child, fixture_status *status, int options);

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-nullable-integer.h"

fixture_child fixture_wait(fixture_child child, fixture_status *status, int options)
{
    if (status) {
        *status += options;
        return child;
    }
    return child + options;
}

fixture_child fixture_alternate_wait(fixture_child child, fixture_status *status, int options)
{
    (void)child;
    (void)status;
    (void)options;
    return -1;
}

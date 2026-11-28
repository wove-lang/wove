// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-literal-array.h"
#include <stdio.h>

// Write the two original array elements only after valid flags arrive.
int fixture_fill(fixture_descriptor descriptors[2], int flags)
{
    if (flags < 0) {
        fputs("short view entered the native function\n", stderr);
        return -1;
    }
    descriptors[0] = flags + 1;
    descriptors[1] = flags + 2;
    return 0;
}

// Make an accidental context-dependent lookup observable.
int fixture_alternate_fill(fixture_descriptor descriptors[2], int flags)
{
    (void)descriptors;
    (void)flags;
    return -1;
}

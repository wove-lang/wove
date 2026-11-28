// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-mutable-cstr.h"
#include <string.h>

// Return a pointer into the scratch buffer after writing across the copyback extent.
fixture_path fixture_edit(fixture_path path)
{
    if (strlen(path) != 4)
        return NULL;
    // A failing native call can still change the borrowed buffer.
    if (path[0] == 'f') {
        path[0] = 'F';
        return NULL;
    }
    path[0] = 'P';
    path[1] = 'X';
    path[2] = '\0';
    path[3] = 'Q';
    return path + 1;
}

// Make an accidental context-dependent lookup observable.
fixture_path fixture_alternate_edit(fixture_path path)
{
    (void)path;
    return NULL;
}

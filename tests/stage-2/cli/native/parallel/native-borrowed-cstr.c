// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-borrowed-cstr.h"
#include <string.h>

static char slot[7] = "first";

// Return either absent, empty, or populated C-owned storage.
fixture_text fixture_lookup(const char *name)
{
    if (strcmp(name, "present") == 0)
        return slot;
    if (strcmp(name, "empty") == 0)
        return "";
    return NULL;
}

// Make an accidental context-dependent lookup observable.
fixture_text fixture_alternate_lookup(const char *name)
{
    (void)name;
    return "wrong";
}

// Overwrite the buffer already returned by the first lookup.
int fixture_replace(void)
{
    memcpy(slot, "after!", sizeof slot);
    return 0;
}

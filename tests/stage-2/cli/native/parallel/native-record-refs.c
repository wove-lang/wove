// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-record-refs.h"
#include <string.h>

static const fixture_info *captured;

int fixture_capture(const char *path, fixture_info *info)
{
    if (strcmp(path, "entry") != 0)
        return -1;
    info->value = 42;
    info->cookie = 13;
    captured = info;
    return 0;
}

int fixture_other_capture(const char *path, fixture_info *info)
{
    (void)path;
    info->value = -100;
    return 0;
}

int fixture_update(int value, fixture_info *info)
{
    info->value = value;
    return 23;
}

int fixture_read(const fixture_info *info)
{
    return info == captured && info->cookie == 13 ? info->value : -1;
}

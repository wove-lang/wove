// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-pointer-owner.h"

#include <stdio.h>
#include <string.h>

struct fixture_stream {
    int active;
};

static struct fixture_stream stream;
static int opened;
static int failed;
static int closed;

fixture_handle fixture_open(const char *restrict path, const char *restrict mode)
{
    if (strcmp(mode, "rb")) {
        fputs("invalid mode reached C\n", stderr);
        return NULL;
    }
    if (!strcmp(path, "missing")) {
        failed += 1;
        return NULL;
    }
    if (strcmp(path, "native stream") || stream.active) {
        fputs("invalid path reached C\n", stderr);
        return NULL;
    }
    stream.active = 1;
    opened += 1;
    return &stream;
}

int fixture_close(fixture_handle handle)
{
    if (handle != &stream || !handle->active) {
        closed = -1;
        return -1;
    }
    handle->active = 0;
    closed += 1;
    return 0;
}

fixture_handle fixture_alternate_open(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}

int fixture_alternate_close(fixture_handle handle)
{
    (void)handle;
    return -1;
}

int fixture_stream_counts(void)
{
    return stream.active ? -1 : opened * 100 + failed * 10 + closed;
}

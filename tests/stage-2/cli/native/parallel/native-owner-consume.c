// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-owner-consume.h"

#include <stddef.h>

struct fixture_resource {
    int status;
    int live;
};

static struct fixture_resource current;
static int drops;

// Reuse one address so an extra drop is observable even after the first close.
fixture_resource_handle fixture_open_resource(int status)
{
    if (current.live) {
        return NULL;
    }
    current = (struct fixture_resource){status, 1};
    return &current;
}

// The native destructor consumes the resource on either return status.
int fixture_close_resource(fixture_resource_handle resource)
{
    if (resource != &current || !resource->live) {
        drops = -100;
        return -1;
    }
    resource->live = 0;
    ++drops;
    return resource->status;
}

// Return an error sentinel if a second destruction was attempted.
int fixture_resource_drops(void)
{
    return drops;
}

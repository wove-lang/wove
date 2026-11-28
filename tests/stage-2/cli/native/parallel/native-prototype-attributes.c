// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-prototype-attributes.h"

#include <stdlib.h>

static int live;

fixture_block fixture_allocate_aligned(unsigned long alignment, unsigned long count, unsigned long bytes)
{
    fixture_block block = aligned_alloc(alignment, count * bytes);
    if (block)
        ++live;
    return block;
}

fixture_block fixture_allocate(unsigned long bytes)
{
    return fixture_allocate_aligned(16, 1, bytes);
}

int fixture_release(fixture_block block)
{
    free(block);
    --live;
    return 0;
}

int fixture_live(void)
{
    return live;
}

int fixture_score(int value)
{
    return value * 3;
}

int fixture_legacy(int value)
{
    return value;
}

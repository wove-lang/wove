// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-bytes.h"

int fixture_sum(const unsigned char *data, fixture_size width, fixture_size count)
{
    int total = 0;
    for (fixture_size index = 0; index < width * count; ++index)
        total += data[index];
    return total;
}

int fixture_other_sum(const unsigned char *data, fixture_size width, fixture_size count)
{
    return -fixture_sum(data, width, count);
}

int fixture_fill(void *data, fixture_size width, fixture_size count)
{
    unsigned char *output = data;
    for (fixture_size index = 0; index < width * count; ++index)
        output[index] = (unsigned char)(index + 10);
    return (int)count;
}

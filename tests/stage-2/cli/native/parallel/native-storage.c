// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-storage.h"
#include <stdint.h>

static const fixture_storage *live;
static int drops;

int fixture_init(fixture_storage *storage, int value)
{
    if ((uintptr_t)storage % _Alignof(fixture_storage) != 0 || storage->value != 0)
        return 91;
    if (value == 0)
        return 17;
    storage->value = value;
    live = storage;
    return 0;
}

int fixture_read(const fixture_storage *storage)
{
    return storage == live ? storage->value : -1;
}

int fixture_increment(fixture_storage *storage)
{
    return storage == live ? ++storage->value : -1;
}

int fixture_destroy(fixture_storage *storage)
{
    if (storage != live || storage->value == 0) {
        drops = -100;
        return -1;
    }
    ++drops;
    storage->value = 0;
    live = 0;
    return 23;
}

int fixture_other_destroy(fixture_storage *storage)
{
    int status = fixture_destroy(storage);
    drops = -200;
    return status;
}

int fixture_current(void)
{
    return live ? live->value : 0;
}

int fixture_drops(void)
{
    return drops;
}

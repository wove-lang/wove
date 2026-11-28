// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-field-snapshot.h"

#include <stddef.h>
#include <string.h>

struct fixture_directory {
    int mode;
    int reads;
    int live;
};

struct fixture_entry_storage {
    struct fixture_entry entry;
    char extension[6];
};

static struct fixture_directory directory;
static struct fixture_entry_storage current;
static int drops;

_Static_assert(offsetof(struct fixture_entry_storage, extension)
    == offsetof(struct fixture_entry, name) + sizeof(current.entry.name),
    "the record extends the nominal name without padding");

fixture_handle fixture_open_directory(int mode)
{
    if (directory.live) {
        return NULL;
    }
    directory = (struct fixture_directory){mode, 0, 1};
    return &directory;
}

int fixture_close_directory(fixture_handle handle)
{
    if (handle != &directory || !handle->live) {
        drops = -100;
        return -1;
    }
    handle->live = 0;
    ++drops;
    return 0;
}

const struct fixture_entry *fixture_read_entry(fixture_handle handle)
{
    if (handle != &directory || !handle->live) {
        return NULL;
    }
    ++handle->reads;
    if (handle->reads > 2) {
        return NULL;
    }
    if (handle->mode == 1) {
        current.entry.extent = offsetof(struct fixture_entry, name);
        return &current.entry;
    }
    if (handle->mode == 2) {
        memset(current.entry.name, 'x', sizeof(current.entry.name));
        memset(current.extension, 'x', sizeof(current.extension));
        current.entry.extent = sizeof(current);
        return &current.entry;
    }
    if (handle->reads == 1) {
        memcpy(current.entry.name, "long", sizeof(current.entry.name));
        memcpy(current.extension, "-name", sizeof(current.extension));
        current.entry.extent = sizeof(current);
    } else {
        memcpy(current.entry.name, "next", sizeof(current.entry.name));
        memset(current.extension, 0, sizeof(current.extension));
        current.entry.extent = sizeof(current);
    }
    return &current.entry;
}

int fixture_directory_drops(void)
{
    return directory.live ? -1 : drops;
}

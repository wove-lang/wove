// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_FIELD_SNAPSHOT_H
#define WOVE_PARALLEL_NATIVE_FIELD_SNAPSHOT_H

typedef struct fixture_directory *fixture_handle;

struct fixture_entry {
#if defined(WOVE_FIXTURE_SHIFT_LAYOUT) && __has_include("unit-0000000.c")
    unsigned char prefix[5];
    unsigned int extent;
    char name[8];
#else
    unsigned char prefix;
    unsigned short extent;
    char name[4];
#endif
};

fixture_handle fixture_open_directory(int mode);
int fixture_close_directory(fixture_handle handle);

const struct fixture_entry *fixture_read_entry(fixture_handle handle)
#if defined(__APPLE__)
    __asm__("_fixture_native_read_entry")
    __attribute__((availability(macos, introduced = 10.5)))
#else
    __asm__("fixture_native_read_entry")
#endif
    __attribute__((nothrow, nonnull(1)));

int fixture_directory_drops(void);

#endif

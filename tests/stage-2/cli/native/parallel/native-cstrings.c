// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-cstrings.h"

int fixture_mkdir(const char *path, fixture_path_mode mode) {
    return path[0] == 'd' && path[1] == '\0' && mode == 0640 ? 23 : -1;
}

int fixture_unlink(const char *path) {
    return path[0] == 'd' && path[1] == '\0' ? 0 : -1;
}

int fixture_rename(const char *from, const char *to) {
    return from[0] == 'p' && from[1] == '\0' && to[0] == 'q' && to[1] == '\0' ? 42 : -1;
}

int fixture_alternate_rename(const char *from, const char *to) {
    (void)from;
    (void)to;
    return 99;
}

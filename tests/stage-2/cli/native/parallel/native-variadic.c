// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdarg.h>
#include "native-variadic.h"

int fixture_open(const char *path, int flags, ...) {
    va_list arguments;
    va_start(arguments, flags);
#if defined(__APPLE__)
    int mode = va_arg(arguments, int);
#else
    unsigned int mode = va_arg(arguments, unsigned int);
#endif
    va_end(arguments);
    if (path[0] != 'p' || path[1] != '\0') {
        return 1;
    }
    if (flags == 13 && mode == 0600) {
        return 23;
    }
    return flags == 17 && mode == 0640 ? 42 : 2;
}

int fixture_other_open(const char *path, int flags, ...) {
    (void)path;
    (void)flags;
    return 99;
}

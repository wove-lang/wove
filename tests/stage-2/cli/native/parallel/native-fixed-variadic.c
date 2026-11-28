// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-fixed-variadic.h"
#include <stdarg.h>

// Use the real command and read the promoted integer passed to the ellipsis.
int fixture_control(int descriptor, int command, ...)
{
    va_list arguments;
    va_start(arguments, command);
    const int value = va_arg(arguments, int);
    va_end(arguments);
    if (command == FIXTURE_SET)
        return descriptor + value;
    if (command == FIXTURE_DUP)
        return descriptor - value;
    return -1;
}

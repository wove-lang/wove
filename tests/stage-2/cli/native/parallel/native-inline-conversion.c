// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define WOVE_INLINE_CONVERSION_IMPLEMENTATION
#include "native-inline-conversion.h"

#include <stdlib.h>

const char *fixture_negative_text = "-42";
const char *fixture_long_text = "123456789";

void fixture_drop_text(const char *text) {
    (void)text;
}

long fixture_strtol(const char *text, char **end, int base) {
    return strtol(text, end, base);
}

int fixture_as_int(const char *text) {
    return (int)fixture_strtol(text, (char **)((void *)0), 10);
}

long fixture_as_long(const char *text) {
    return fixture_strtol(text, (char **)((void *)0), 10);
}

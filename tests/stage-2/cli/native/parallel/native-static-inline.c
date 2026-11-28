// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-static-inline.h"

unsigned fixture_observe_swap32(unsigned value, unsigned (*helper)(unsigned)) {
    static unsigned (*first)(unsigned);
    if (!first)
        first = helper;
    return first == helper ? fixture_swap32(helper(value)) : 0;
}

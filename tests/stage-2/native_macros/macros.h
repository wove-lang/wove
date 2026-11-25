// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_MACROS_H
#define WOVE_TEST_MACROS_H

#include <stdint.h>

#define MACRO_REPEAT(first, second) ((uint32_t)(first) + (first) + (second))
#define MACRO_IGNORE(value) UINT32_MAX
#define MACRO_IS_BYTE(value) _Generic((value), uint8_t: 1, default: 0)

#endif

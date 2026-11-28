// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_FIXTURE_INTEGER_MACRO_H
#define WOVE_FIXTURE_INTEGER_MACRO_H

#define FIXTURE_SUFFIX(value) (value ## ULL)
#define FIXTURE_SIGNED(value) ((int)(value))
#define FIXTURE_SMALL(value) ((int)(value))
#define FIXTURE_MIX(value, first, ignored) ((value ## ULL) + (first) + (first))
#define FIXTURE_REPEAT(first, second) ((int)(first) + (first) + (second))
#define FIXTURE_BOUNDED(data, count) ((count) == 1 ? (int)((const unsigned char *)(data))[0] : -1)
#define FIXTURE_WRONG_RESULT(value) (value ## ULL)

#endif

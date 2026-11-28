// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_LONG_DOUBLE_TYPEDEF_H
#define WOVE_TEST_LONG_DOUBLE_TYPEDEF_H

#if defined(DOUBLE_ALIAS)
typedef double FixtureLongDouble;
#elif defined(CONST_ALIAS)
typedef const long double FixtureLongDouble;
#elif !defined(OMIT_ALIAS)
typedef long double FixtureLongBase;
typedef FixtureLongBase FixtureLongDouble;
#endif

#endif

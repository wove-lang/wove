// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_MACRO_H
#define WOVE_PARALLEL_NATIVE_MACRO_H

typedef unsigned short fixture_mode;

#define FIXTURE_BONUS 33
#if defined(FIXTURE_UNSAFE)
#define FIXTURE_SCORE(used, unused) ((((++(used)) - 1) * 3) + FIXTURE_BONUS)
#else
#define FIXTURE_SCORE(used, unused) (((used) * (used)) + FIXTURE_BONUS)
#endif
#define FIXTURE_NONZERO(value) ((value) != 0)

#endif

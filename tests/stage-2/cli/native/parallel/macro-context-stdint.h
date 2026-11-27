// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_MACRO_CONTEXT_STDINT_H
#define WOVE_PARALLEL_MACRO_CONTEXT_STDINT_H
#pragma clang system_header
#include_next <stdint.h>
#if __INCLUDE_LEVEL__ == 3
#undef FIXTURE_SCORE
#define FIXTURE_SCORE(used, unused) ((used) + FIXTURE_BONUS)
#endif
#endif

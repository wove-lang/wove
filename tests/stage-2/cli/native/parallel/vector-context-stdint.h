// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma clang system_header
#include_next <stdint.h>

#if __INCLUDE_LEVEL__ == 3
#undef fixture_vector
#define fixture_vector fixture_spare
#endif

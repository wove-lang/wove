// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma clang system_header
#include_next <stdint.h>

#if __INCLUDE_LEVEL__ == 3
extern int fixture_rename(const char *from, const char *to) __attribute__((nonnull(2)));
#endif

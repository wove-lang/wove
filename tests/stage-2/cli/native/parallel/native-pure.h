// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_PURE_H
#define WOVE_PARALLEL_NATIVE_PURE_H

extern double atof(const char *text) __attribute__((pure, nothrow, nonnull(1)));
extern double atof(const char *text);

#endif

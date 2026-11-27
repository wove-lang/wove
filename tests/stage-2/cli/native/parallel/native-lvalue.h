// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_LVALUE_H
#define WOVE_PARALLEL_NATIVE_LVALUE_H

extern int *fixture_slot(void) __attribute__((nothrow, const));
extern int *fixture_other_slot(void);
extern void fixture_barrier(void);

#define fixture_lvalue (*fixture_slot())

#endif

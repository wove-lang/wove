// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_VECTOR_H
#define WOVE_PARALLEL_NATIVE_VECTOR_H

extern char **fixture_rows;
extern char **fixture_spare;
extern void fixture_advance(void);

#define fixture_vector fixture_rows

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_DEPENDENCY_H
#define WOVE_PARALLEL_DEPENDENCY_H

typedef int fixture_pid;

#if DEPENDENCY_CONTEXT == 1
typedef char dependency_source_name[sizeof(__BASE_FILE__)];
#endif

#endif

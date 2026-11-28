// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_FIXTURE_ORDINARY_H
#define WOVE_FIXTURE_ORDINARY_H

#if __STDC_VERSION__ != 201710L
#error This header requires the tool job's resolved C17 mode.
#endif

#if BRIDGE_MODE == 1
static int local_state __attribute__((used));
#elif BRIDGE_MODE == 2
typedef char source_filename[sizeof(__BASE_FILE__)];
#endif

#if BRIDGE_MODE == 3
extern int getpid(void) __attribute__((nothrow));
#else
extern int getpid(void);
#endif

#endif

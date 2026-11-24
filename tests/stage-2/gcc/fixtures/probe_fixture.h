// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_GCC_PROBE_FIXTURE_H
#define WOVE_GCC_PROBE_FIXTURE_H

typedef unsigned long ProbeWord;
extern int probe_variable;
ProbeWord probe_function(ProbeWord value, ...);

// GCC also reports header functions that only have a definition.
static inline int probe_inline(void) {
	int probe_local = 1;
	return probe_local;
}

#endif

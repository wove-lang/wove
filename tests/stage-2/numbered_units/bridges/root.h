// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#if WOVE_NUMBERED_FEATURE == 7 && __STDC_VERSION__ == 201112L
extern int bridge_root(void);
#elif WOVE_NUMBERED_FEATURE == 8
#define WOVE_NUMBERED_FUNCTION(value) (value)
extern int bridge_root(void);
#else
static inline int bridge_root(void) { return 17; }
#endif

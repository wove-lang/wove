// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#if __INCLUDE_LEVEL__ != 1
#error native prefix must have serial include depth
#endif
#define WOVE_NUMBERED_CONTEXT 1
extern int bridge_context(void);

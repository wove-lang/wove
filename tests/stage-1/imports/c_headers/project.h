// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Fail the C build if two declarations emit the same project header.
#ifdef WOVE_STAGE1_PROJECT_HEADER_SEEN
#error "the project header was included twice"
#endif
#define WOVE_STAGE1_PROJECT_HEADER_SEEN 1

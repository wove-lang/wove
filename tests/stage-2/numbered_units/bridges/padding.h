// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// A single unused integer token grows the preprocessed output without growing the AST.
#if __has_include("unit-0000001.c")
#define WOVE_PADDING 0WOVE_NUMBERED_BYTES
#else
#define WOVE_PADDING 0WOVE_SERIAL_BYTES
#endif

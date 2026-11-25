// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "leaf/value.h"

// The header opened through the symlink also resolves its own project include.
#if WOVE_HEADER_LOOKUP_VALUE != 42
#error "the project header selected the wrong relative include"
#endif

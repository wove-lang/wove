// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#if __STDC_VERSION__ != 201710L
#error expected C17
#endif

// Resolve this definition across object files.
int staged_leaf(void) {
	return 37;
}

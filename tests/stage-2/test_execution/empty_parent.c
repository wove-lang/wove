// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// An empty aggregate run must succeed without entering the parent.
int wvrt_test_parent(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return 1;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>

#undef main
int wove_emitted_main(int argc, char **argv);

// Report the full return value without truncating it to a process exit code.
int main(int argc, char **argv) {
	return printf("%d\n", wove_emitted_main(argc, argv)) < 0;
}

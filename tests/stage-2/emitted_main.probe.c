// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>

#undef main
int wove_emitted_main(void);

// Report the full return value without truncating it to a process exit code.
int main(void) {
	return printf("%d\n", wove_emitted_main()) < 0;
}

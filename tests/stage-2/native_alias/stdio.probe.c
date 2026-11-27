// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>
#include <stdio.h>

#undef main

int wove_fixture_main(int argc, char **argv);
size_t wgf0b87498retain_size(size_t);

// Pass a full-width value through the alias selected from the checked-in stdio bridge.
int main(int argc, char **argv) {
	if (wgf0b87498retain_size(SIZE_MAX) != SIZE_MAX) { return 1; }
	return wove_fixture_main(argc, argv);
}

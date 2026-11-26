// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>

#undef main

int32_t wgF08vpa130000001parse(void);
int32_t wgF08vpa130000002parse(void);
int wove_collision_main(int argc, char **argv);

// The original providers keep the same symbol assignments in either graph order.
int main(int argc, char **argv) {
	if (wgF08vpa130000001parse() != 23 || wgF08vpa130000002parse() != 19) {
		return 1;
	}
	return wove_collision_main(argc, argv);
}

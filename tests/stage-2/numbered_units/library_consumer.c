// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <libhistogram.h>

int check_views(void);

// Call the scalar export and run the existing checks for every checked view export.
int main(void) {
	return numbered_answer(25) == 42 ? check_views() : 1;
}

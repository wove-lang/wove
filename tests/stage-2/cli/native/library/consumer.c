/*
 * Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <libscalars.h>

/* Call each public C prototype through the dynamic library. */
int main(void) {
	Reset();
	/* Imported spelling, fixed-width arguments, Boolean and floating results cross the ABI. */
	if (!Mix(-8, 8, -16, 16, -32, 32, -64, 64, 3.25f, 42.5, true)
			|| Mix(-8, 8, -16, 16, -32, 32, -64, 64, 3.25f, 42.5, false)
			|| Half(85.0) != 42.5) {
		return 1;
	}
	return Answer() + Double(1) - 1;
}

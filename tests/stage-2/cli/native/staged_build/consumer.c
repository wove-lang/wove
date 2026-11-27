// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

int staged_answer(void);

// Call the exported function through the linked library.
int main(void) {
	return staged_answer();
}

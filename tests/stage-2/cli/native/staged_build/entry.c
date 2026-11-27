// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <limits.h>

_Static_assert(CHAR_BIT == 8, "expected byte-addressed native target");

#if __STDC_VERSION__ != 201710L
#error expected C17
#endif

int staged_leaf(void);

#ifdef STAGED_LIBRARY
// Export only the entry from the linked library.
__attribute__((visibility("default"))) int staged_answer(void) {
	return staged_leaf() + 5;
}
#else
// Exercise a definition in the other translation unit.
int main(void) {
	return staged_leaf() + 5;
}
#endif

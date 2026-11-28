// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_REPLAY_DEPENDENCY_H
#define WOVE_PARALLEL_NATIVE_REPLAY_DEPENDENCY_H

extern int fixture_uncertified_control(int value);
extern int fixture_uncertified(int value);

#if defined(FIXTURE_HELPER)
// The forwarding expression resolves the earlier ordinary prototype.
extern inline __attribute__((gnu_inline)) int fixture_forward(int value) {
	return fixture_uncertified(value);
}
#endif

extern int fixture_uncertified(int value) __attribute__((returns_twice));

#if defined(FIXTURE_CALLBACK)
extern int fixture_apply(int (*operation)(int), int value);

extern inline __attribute__((gnu_inline)) int fixture_forward(int value) {
	int (*callback)(int) = &fixture_uncertified;
	return fixture_apply(callback, value);
}
#endif

#if defined(FIXTURE_RUNTIME)
extern __SIZE_TYPE__ strlen(const char *text) __attribute__((returns_twice));
#endif

extern inline __attribute__((gnu_inline)) int fixture_unused_inline(int value) {
	if (value > 0) {
		return value;
	}
	return 0;
}

#endif

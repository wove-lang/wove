// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_RECORD_INIT_FIXTURE_H
#define WOVE_RECORD_INIT_FIXTURE_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t wove_word;
typedef int32_t wove_other_word;

struct wove_child {
	int32_t marker;
};

struct wove_words {
	wove_word first;
	int32_t second;
	int32_t visible_zero;
	struct wove_child nested;
	void *hidden;
};

static int32_t calls;

// Return the next observable value through the declared native alias.
static inline wove_word next_word(void) { return ++calls * 11; }

// Return the next value through the built-in Wove integer type.
static inline int32_t next_number(void) { return ++calls * 11; }

// Check the initialized fields and the native members outside the initializer list.
static inline int32_t verify_words(const struct wove_words *words) {
	return calls == 2 && words->first == 22 && words->second == 11 &&
		words->visible_zero == 0 && words->nested.marker == 0 && words->hidden == NULL ? 42 : 1;
}

#endif

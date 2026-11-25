// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdint.h>

typedef struct {
	uint8_t lead;
	_Alignas(64) uint8_t hidden[81];
	uint64_t value;
} NativeLayout;

// Verify alignment and initial members before writing to the complete object.
static inline int32_t fill_layout(NativeLayout *out) {
	if ((uintptr_t)out % _Alignof(NativeLayout) || out->lead != 0 || out->value != 0) { return 1; }
	// The record view leaves the hidden array in its original layout.
	for (unsigned i = 0; i < sizeof out->hidden; ++i) {
		if (out->hidden[i] != 0) { return 2; }
	}
	out->hidden[80] = 255;
	out->value = UINT64_MAX;
	return 0;
}

// Read the original contents through a shared C borrow.
static inline int32_t check_layout_contents(const NativeLayout *info) {
	return info->hidden[80] == 255 ? 42 : 3;
}

// Require the generated shared argument to retain its const qualification.
#define check_layout(info) _Generic((info), const NativeLayout *: check_layout_contents)(info)

// Supply the expected value at the native type's full width.
static inline uint64_t max_layout_value(void) {
	return UINT64_MAX;
}

// Read one object and write another without invalidating the shared view.
static inline int32_t copy_layout(const NativeLayout *from, NativeLayout *to) {
	*to = *from;
	return 0;
}

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_VARIADIC_H
#define WOVE_TEST_VARIADIC_H

#include <stdarg.h>
#include <stdint.h>

// Read the target's promoted scalar types and the converted pointer from the native tail.
static inline int32_t test_promotions(int32_t fixed, ...) {
	va_list args;
	va_start(args, fixed);
	int flag = va_arg(args, int);
	int byte = va_arg(args, int);
	int small = va_arg(args, int);
	uint32_t wide = va_arg(args, uint32_t);
	const char *text = va_arg(args, const char *);
	va_end(args);
	return fixed == -7 && flag == 1 && byte == 255 && small == 65535 &&
		wide == UINT32_MAX && text[0] == '*' && text[1] == '\0' ? 42 : 1;
}

// Read a typed shared byte pointer and the promoted integer from the tail.
static inline int32_t read_first(const uint8_t *bytes, int32_t count, ...) {
	va_list args;
	va_start(args, count);
	int expected = va_arg(args, int);
	va_end(args);
	return count == 1 && bytes[0] == expected ? 42 : 1;
}

// Copy through mixed byte pointer types with independent access qualifiers.
static inline int32_t copy_first(const volatile signed char *bytes, char *target, int32_t count, ...) {
	va_list args;
	va_start(args, count);
	int expected = va_arg(args, int);
	va_end(args);
	if (count != 1 || bytes[0] != expected) { return 1; }
	target[0] = (char)bytes[0];
	return 42;
}

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_LOCALE_MACROS_H
#define WOVE_TEST_LOCALE_MACROS_H

#include <ctype.h>
#include <locale.h>
#include <stdint.h>

// The Darwin fixture reproduces glibc's comma operand using the host locale API.
#if defined(__APPLE__)
#include <xlocale.h>
#define isascii_l(character, locale) ((locale), isascii(character))
#define toascii_l(character, locale) ((locale), toascii(character))
#endif

static int32_t locale_opens;
static int32_t locale_drops;
static int32_t character_reads;

// Acquire a real locale and count each successful acquisition.
static inline locale_t tracked_newlocale(int mask, const char *name, locale_t base) {
	locale_t owner = newlocale(mask, name, base);
	locale_opens += owner != NULL;
	return owner;
}

// Release a real locale and record cleanup separately from acquisition.
static inline void tracked_freelocale(locale_t owner) {
	(void)freelocale(owner);
	++locale_drops;
}

// Count the source operand's evaluations independently of the macro.
static inline int32_t next_character(int32_t character) {
	++character_reads;
	return character;
}

// Check exactly one complete owner lifetime and one evaluation per call.
static inline int32_t tracked_locale_ok(void) {
	return locale_opens == 1 && locale_drops == 1 && character_reads == 4;
}

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <locale.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

_Static_assert(_Generic(&strlen, size_t (*)(const char *): 1, default: 0), "native strlen prototype");
_Static_assert(_Generic(&strcmp, int (*)(const char *, const char *): 1, default: 0), "native strcmp prototype");
_Static_assert(_Generic(&strncmp, int (*)(const char *, const char *, size_t): 1, default: 0), "native strncmp prototype");
_Static_assert(_Generic(&strcoll, int (*)(const char *, const char *): 1, default: 0), "native strcoll prototype");
_Static_assert(_Generic(&strerror, char *(*)(int): 1, default: 0), "native strerror prototype");

static char saved[512];

// Fix the fixture's collation locale before comparing ordering signs.
static inline bool fixture_c_collation(void)
{
	return setlocale(LC_COLLATE, "C") != NULL;
}

// Save an independent copy of the native message before the Wove snapshots.
static inline bool fixture_save_error(int32_t error)
{
	const char *message = strerror(error);
	// Bound the copy by the fixture's storage capacity.
	if (message == NULL || strlen(message) >= sizeof(saved)) {
		return false;
	}
	memcpy(saved, message, strlen(message) + 1);
	return true;
}

// Compare the retained snapshot with the value saved before the later call.
static inline bool fixture_matches_saved(const char *text)
{
	return strcmp(text, saved) == 0;
}

// Compare the second snapshot with the native message for its own error.
static inline bool fixture_matches_error(const char *text, int32_t error)
{
	const char *message = strerror(error);
	return message != NULL && strcmp(text, message) == 0;
}

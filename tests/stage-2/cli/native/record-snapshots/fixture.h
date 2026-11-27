/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct fixture_record {
	uint32_t number;
	char *name;
	char *password;
	char **members;
};

static inline struct fixture_record *fixture_snapshot(int32_t input)
{
	static _Thread_local struct fixture_record result;
	static _Thread_local char name[4];
	static _Thread_local char first[3];
	static _Thread_local char second[1];
	static _Thread_local char third[4];
	static _Thread_local char password[1];
	_Alignas(char *) static _Thread_local unsigned char packed[sizeof(char *) * 4 + 1];

	if (input < 0) {
		return NULL;
	}
	if (input == 0) {
		memcpy(name, "abc", sizeof name);
		memcpy(first, "de", sizeof first);
		memcpy(third, "fgh", sizeof third);
		char *rows[] = {first, second, third, NULL};
		memcpy(packed + 1, rows, sizeof rows);
		result = (struct fixture_record){42, name, NULL, (char **)(uintptr_t)(packed + 1)};
	} else {
		memset(name, 0, sizeof name);
		memset(first, 0, sizeof first);
		memset(third, 0, sizeof third);
		memset(packed, 0, sizeof packed);
		result = (struct fixture_record){1, name, password, NULL};
	}
	return &result;
}

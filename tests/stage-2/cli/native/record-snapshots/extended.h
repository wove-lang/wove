// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_FIXTURE_EXTENDED_RECORD_H
#define WOVE_FIXTURE_EXTENDED_RECORD_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef WOVE_FIXTURE_EXTENT_TYPE
#define WOVE_FIXTURE_EXTENT_TYPE uint16_t
#endif
#ifndef WOVE_FIXTURE_TEXT_TYPE
#define WOVE_FIXTURE_TEXT_TYPE char
#endif

struct fixture_extent_record {
	uint32_t marker;
	WOVE_FIXTURE_EXTENT_TYPE extent;
	WOVE_FIXTURE_TEXT_TYPE text[3];
};

struct fixture_extent_reader {
	struct fixture_extent_record *entry;
	int32_t mode;
	unsigned int reads;
};

// Allocate storage for a native record whose inline text can extend past its nominal field.
static inline struct fixture_extent_reader *fixture_extent_open(int32_t mode)
{
	const size_t capacity = offsetof(struct fixture_extent_record, text) + sizeof("extended payload");
	struct fixture_extent_reader *reader = malloc(sizeof(*reader));
	struct fixture_extent_record *entry = malloc(capacity);
	if (reader == NULL || entry == NULL) { abort(); }
	*reader = (struct fixture_extent_reader){.entry = entry, .mode = mode};
	*entry = (struct fixture_extent_record){.marker = 1234, .extent = (uint16_t)capacity};
	return reader;
}

// Release the backing record and its owner independently of any completed snapshot.
static inline void fixture_extent_close(struct fixture_extent_reader *reader)
{
	free(reader->entry);
	free(reader);
}

// Return one of the controlled record shapes from storage reused by the next call.
static inline struct fixture_extent_record *fixture_extent_next(struct fixture_extent_reader *reader)
{
	if (reader->mode == 1) {
		errno = ENOENT;
		return NULL;
	}
	struct fixture_extent_record *entry = reader->entry;
	const size_t offset = offsetof(struct fixture_extent_record, text);
	char *text = (char *)entry + offset;
	const char *value = reader->reads++ == 0 ? "extended payload" : "reused";
	size_t length = strlen(value);
	if (reader->mode == 4) { length = 0; }
	memcpy(text, value, length);
	text[length] = '\0';
	entry->extent = (uint16_t)(offset + length + 1);
	if (reader->mode == 2) { entry->extent = (uint16_t)(offset - 1); }
	if (reader->mode == 3) { entry->extent = (uint16_t)(offset + length); }
	errno = EAGAIN;
	return entry;
}

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <dirent.h>
#include <stdint.h>

// Preserve the stream-owned entry with a read-only native pointer type.
static inline const struct dirent *read_const_entry(DIR *unrelated, DIR *stream) {
	(void)unrelated;
	return readdir(stream);
}

// Read through the same native address supplied by a Wove shared reborrow.
static inline uint16_t entry_reclen(const struct dirent *entry) {
	return entry->d_reclen;
}

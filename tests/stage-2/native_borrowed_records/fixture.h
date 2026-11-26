// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <dirent.h>

// Preserve the stream-owned entry with a read-only native pointer type.
static inline const struct dirent *read_const_entry(DIR *unrelated, DIR *stream) {
	(void)unrelated;
	return readdir(stream);
}

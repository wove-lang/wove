// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_NATIVE_HANDLE_OUT_SQLITE_H
#define WOVE_NATIVE_HANDLE_OUT_SQLITE_H

#include <stddef.h>
#include <sqlite3.h>

// Expose the statement output while supplying SQLite's optional SQL tail argument.
static inline int wove_sqlite_prepare(sqlite3 *db, const char *sql, int length, sqlite3_stmt **output) {
	return sqlite3_prepare_v2(db, sql, length, output, NULL);
}

#endif

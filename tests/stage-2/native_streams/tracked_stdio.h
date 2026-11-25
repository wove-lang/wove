// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_TRACKED_STDIO_H
#define WOVE_TEST_TRACKED_STDIO_H

#include <stdio.h>

FILE *wove_test_fopen(const char *path, const char *mode);
int wove_test_fclose(FILE *stream);
#define fopen wove_test_fopen
#define fclose wove_test_fclose

#endif

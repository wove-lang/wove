// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_SNAPSHOT_CALL_H
#define WOVE_PARALLEL_NATIVE_SNAPSHOT_CALL_H

char *fixture_resolve(const char *path, char *output);
void fixture_release(char *path);
void fixture_other_release(char *path);
int fixture_releases(void);

#endif

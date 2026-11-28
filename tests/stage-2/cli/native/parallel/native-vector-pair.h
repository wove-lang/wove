// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_VECTOR_PAIR_H
#define WOVE_PARALLEL_NATIVE_VECTOR_PAIR_H

typedef int fixture_pid;
typedef struct {
    _Alignas(32) int active;
} fixture_actions;
typedef struct fixture_attribute *fixture_attributes;

int fixture_actions_init(fixture_actions *actions);
int fixture_actions_drop(fixture_actions *actions);
int fixture_attributes_init(fixture_attributes *attributes);
int fixture_attributes_drop(fixture_attributes *attributes);
int fixture_spawn_drops(void);

int fixture_spawn(fixture_pid *restrict pid, const char *restrict file,
                  const fixture_actions *actions, const fixture_attributes *restrict attributes,
                  char *const *restrict arguments, char *const *restrict environment)
#if defined(__APPLE__)
    __asm__("_fixture_native_spawn")
    __attribute__((availability(macos, introduced = 10.5)))
#else
    __asm__("fixture_native_spawn")
#endif
    __attribute__((nothrow, nonnull(1, 2, 5, 6)));

int fixture_alternate_spawn(fixture_pid *pid, const char *file, const fixture_actions *actions,
                            const fixture_attributes *attributes, char *const *arguments,
                            char *const *environment);

#endif

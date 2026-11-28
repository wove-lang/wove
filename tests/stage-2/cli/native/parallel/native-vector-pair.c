// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-vector-pair.h"

#include <stdio.h>
#include <string.h>

struct fixture_attribute {
    int active;
};

static fixture_actions *original_actions;
static fixture_attributes *original_attributes;
static struct fixture_attribute native_attribute = { 71 };
static int drops;

int fixture_actions_init(fixture_actions *actions)
{
    actions->active = 37;
    original_actions = actions;
    return 0;
}

int fixture_actions_drop(fixture_actions *actions)
{
    if (actions != original_actions || actions->active != 37) {
        drops = -10;
        return -1;
    }
    original_actions = NULL;
    drops += 1;
    return 0;
}

int fixture_attributes_init(fixture_attributes *attributes)
{
    *attributes = &native_attribute;
    original_attributes = attributes;
    return 0;
}

int fixture_attributes_drop(fixture_attributes *attributes)
{
    if (attributes != original_attributes || *attributes != &native_attribute) {
        drops = -10;
        return -1;
    }
    original_attributes = NULL;
    drops += 1;
    return 0;
}

int fixture_spawn_drops(void)
{
    return original_actions || original_attributes ? -1 : drops;
}

int fixture_spawn(fixture_pid *restrict pid, const char *restrict file,
                  const fixture_actions *actions, const fixture_attributes *restrict attributes,
                  char *const *restrict arguments, char *const *restrict environment)
{
    if (strcmp(file, "fixture tool") != 0 || (actions && actions != original_actions)
        || (attributes && attributes != original_attributes) || arguments == environment
        || !arguments[0] || !arguments[1] || !arguments[2] || arguments[3]
        || !environment[0] || !environment[1] || environment[2]
        || strcmp(arguments[0], "fixture-child") || strcmp(arguments[1], "space arg")
        || arguments[2][0] || strcmp(environment[0], "FIRST=env value")
        || strcmp(environment[1], "SECOND=distinct")) {
        fputs("unexpected call to the native function\n", stderr);
        return -1;
    }
    arguments[0][0] = 'X';
    environment[0][0] = 'Y';
    const int present = (actions != NULL) + 2 * (attributes != NULL);
    *pid = 100 + present;
    return 17 + present;
}

int fixture_alternate_spawn(fixture_pid *pid, const char *file, const fixture_actions *actions,
                            const fixture_attributes *attributes, char *const *arguments,
                            char *const *environment)
{
    (void)pid;
    (void)file;
    (void)actions;
    (void)attributes;
    (void)arguments;
    (void)environment;
    return -1;
}

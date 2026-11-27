// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-snapshot-call.h"
#include <stdlib.h>
#include <string.h>

static char *live;
static int released;

char *fixture_resolve(const char *path, char *output)
{
    if (output != NULL || live != NULL) {
        released = -100;
        return NULL;
    }
    if (strcmp(path, "first") != 0 && strcmp(path, "second") != 0)
        return NULL;
    const size_t size = strlen(path) + 1;
    live = malloc(size);
    if (live != NULL)
        memcpy(live, path, size);
    return live;
}

void fixture_release(char *path)
{
    if (path == NULL || path != live) {
        released = -101;
        return;
    }
    ++released;
    live = NULL;
    free(path);
}

void fixture_other_release(char *path)
{
    fixture_release(path);
    released = -200;
}

int fixture_releases(void)
{
    return released;
}

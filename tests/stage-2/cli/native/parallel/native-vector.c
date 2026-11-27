// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-vector.h"

static char initial[] = "a";
static char next_first[] = "b";
static char next_second[] = "c";
static char spare[] = "z";
static char *initial_rows[] = {initial, 0};
static char *next_rows[] = {next_first, next_second, 0};
static char *spare_rows[] = {spare, 0};

char **fixture_rows = initial_rows;
char **fixture_spare = spare_rows;
char **fixture_nullable = 0;

void fixture_advance(void)
{
    initial[0] = 'x';
    fixture_rows = next_rows;
}

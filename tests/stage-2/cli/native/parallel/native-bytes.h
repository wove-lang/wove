// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_BYTES_H
#define WOVE_PARALLEL_NATIVE_BYTES_H

typedef __SIZE_TYPE__ fixture_size;

int fixture_sum(const unsigned char *data, fixture_size width, fixture_size count);
int fixture_other_sum(const unsigned char *data, fixture_size width, fixture_size count);
int fixture_fill(void *data, fixture_size width, fixture_size count);

#endif

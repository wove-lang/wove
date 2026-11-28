/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_SEED_LOCAL_TYPES_H
#define WOVE_SEED_LOCAL_TYPES_H

#include <stdint.h>

typedef uint64_t seed_device_t;

struct seed_stat {
	seed_device_t st_dev;
};

#endif

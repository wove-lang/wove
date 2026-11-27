/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_TYPEDEF_FEATURE_H
#define WOVE_TEST_TYPEDEF_FEATURE_H

#if defined(ENABLE_TYPEDEF)
#define SUPPLY_TYPEDEF 1
#if defined(EARLY_TYPEDEF)
#include "fixture.h"
#endif
#elif defined(FOREIGN_TYPEDEF)
typedef unsigned int native_alias_t;
#endif

#endif

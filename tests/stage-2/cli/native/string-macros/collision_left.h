/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef FIXTURE_SEQUENCE
#define FIXTURE_STRINGIFY_INNER(value) #value
#define FIXTURE_STRINGIFY(value) FIXTURE_STRINGIFY_INNER(value)
#define FIXTURE_SEQUENCE FIXTURE_STRINGIFY(__COUNTER__)
#endif

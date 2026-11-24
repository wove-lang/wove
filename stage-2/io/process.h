// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PROCESS_H
#define WOVE_PROCESS_H

#include <stdint.h>

// Fill a stdout prefix and five result bytes (little-endian length, then overflow).
int32_t wove_process_capture(const uint8_t *program, int32_t program_length,
	const uint8_t *arguments, int32_t arguments_length, uint8_t *output, int32_t capacity,
	uint8_t *result, int32_t result_length);

#endif

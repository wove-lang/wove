// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_CLI_HOST_H
#define WOVE_CLI_HOST_H

#include <stdbool.h>
#include <stdint.h>

bool wove_cli_valid_target(const char *target);
int32_t wove_cli_publish(const char *path, const char *input,
	const uint8_t *bytes, int32_t length);

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_CLI_HOST_H
#define WOVE_CLI_HOST_H

#include <stdbool.h>
#include <stdint.h>

int32_t wove_cli_input_size(const char *path);
int32_t wove_cli_read_input(const char *path, uint8_t *bytes, int32_t length);
int32_t wove_cli_asset_size(const char *module, const char *request);
int32_t wove_cli_read_asset(const char *module, const char *request, uint8_t *bytes,
	int32_t length, int32_t start, int32_t end);
int32_t wove_cli_project_size(void);
int32_t wove_cli_read_project(uint8_t *bytes, int32_t length);
bool wove_cli_valid_target(const char *target);
int32_t wove_cli_publish(const char *path, const char *input,
	const uint8_t *bytes, int32_t length);
int32_t wove_cli_stdout(const uint8_t *bytes, int32_t length);
int32_t wove_cli_stderr(const uint8_t *bytes, int32_t length);
int32_t wove_cli_text(uint32_t text, uint8_t *bytes, int32_t capacity);

#endif

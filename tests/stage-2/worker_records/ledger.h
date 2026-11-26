// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_WORKER_RECORD_LEDGER_H
#define WOVE_WORKER_RECORD_LEDGER_H

#include <stdint.h>

int32_t begin(void);
void before_spawn(void);
void before_result(void);

#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "native-lvalue.h"

#include <stdatomic.h>

static _Thread_local int slot;
static _Thread_local int other_slot;
static atomic_int arrivals;

int *fixture_slot(void)
{
	return &slot;
}

int *fixture_other_slot(void)
{
	return &other_slot;
}

void fixture_barrier(void)
{
	atomic_fetch_add_explicit(&arrivals, 1, memory_order_release);
	while (atomic_load_explicit(&arrivals, memory_order_acquire) < 2) {
	}
}

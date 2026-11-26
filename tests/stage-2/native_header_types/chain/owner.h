// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_HEADER_OWNER_H
#define WOVE_TEST_NATIVE_HEADER_OWNER_H

typedef ChainValue *ChainOwner;
static int32_t chain_drops;

// Count cleanup of the handle created in the consumer header.
static inline void release_chain(ChainOwner owner) {
	chain_drops += owner->value == 42;
}

#endif

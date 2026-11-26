// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_NATIVE_HEADER_CONSUMER_H
#define WOVE_TEST_NATIVE_HEADER_CONSUMER_H

static ChainValue chain_value;

// Create a stable native handle to a known value.
static inline ChainOwner make_chain(int32_t value) {
	chain_value.value = value;
	return &chain_value;
}

// Create that handle through a C function with no arguments.
static inline ChainOwner default_chain(void) {
	return make_chain(42);
}

// Read the value through the original owner spelling.
static inline int32_t read_chain(ChainOwner owner) {
	return owner->value;
}

// Let the Wove fixture verify that the resource went through cleanup.
static inline int32_t count_chain_drops(void) {
	return chain_drops;
}

#endif

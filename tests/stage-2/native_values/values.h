// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>

#define TEST_BYTE 255
#define TEST_WIDE UINT64_MAX
#define TEST_TRUE 1

static int32_t test_live_value;
static const int32_t test_readonly = 42;

// Compare the Wove arguments with the actual header macros on this target.
static inline bool check_flags(int32_t close_on_exec, int32_t nonblocking, int32_t no_entry) {
	return close_on_exec == O_CLOEXEC && nonblocking == O_NONBLOCK && no_entry == ENOENT;
}

// Mutate the same thread's C error indicator and return the new value.
static inline int32_t set_errno_for_test(int32_t value) {
	errno = value;
	return errno;
}

// Observe the current C error indicator after a Wove assignment.
static inline int32_t read_errno_for_test(void) {
	return errno;
}

// Change native storage while Wove holds only a read-only binding to it.
static inline int32_t change_live_value(int32_t value) {
	test_live_value = value;
	return test_live_value;
}

static int32_t slots[2];
static unsigned selected_slot;

// Expose whichever C storage the current selector identifies.
static inline int32_t *test_slot_address(void) {
	return &slots[selected_slot];
}
#define TEST_SLOT (*test_slot_address())

// Change the lvalue's address as a side effect of evaluating the value.
static inline int32_t choose_second_slot(void) {
	selected_slot = 1;
	return 42;
}

// Verify that the assignment resolved the lvalue after evaluating its value.
static inline bool slot_matches(void) {
	return slots[0] == 0 && slots[1] == 42;
}

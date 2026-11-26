// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "wove_runtime.h"

#include <string.h>

// Check ABI failures on deliberately malformed descriptors that Wove cannot construct.
int main(void) {
	int32_t output[] = {11, 22, 33, 44};
	const int32_t original[] = {11, 22, 33, 44};
	const int32_t input[] = {0, INT32_MIN, INT32_MAX, 7, 8};
	const size_t width = sizeof(output[0]);
	const wvrt_mut_slice destination = {output, 4, width};
	const wvrt_slice oversized = {input, 5, width};
	const wvrt_slice negative = {input, -1, width};
	const wvrt_slice wrong_stride = {input, 1, 1};
	const wvrt_slice missing = {NULL, 1, width};
	const wvrt_slice overlap_tail = {output + 2, 1, width};
	const wvrt_mut_slice right = {output + 1, 3, width};
	const wvrt_slice overlap_front = {output, 2, width};
	const wvrt_mut_slice overflow = {output, 2, SIZE_MAX};
	const wvrt_slice huge_stride = {input, 1, SIZE_MAX};
	// No rejected length, stride, pointer, or overlapping view may change any element.
	if (wvrt_slice_try_copy(destination, oversized) ||
		wvrt_slice_try_copy(destination, negative) ||
		wvrt_slice_try_copy(destination, wrong_stride) ||
		wvrt_slice_try_copy(destination, missing) ||
		wvrt_slice_try_copy(destination, overlap_tail) ||
		wvrt_slice_try_copy(right, overlap_front) ||
		wvrt_slice_try_copy(overflow, huge_stride) ||
		memcmp(output, original, sizeof(output)) != 0) {
		return 1;
	}

	const wvrt_mut_slice empty_target = {NULL, 0, width};
	const wvrt_slice empty_source = {NULL, 0, width};
	// Empty views can succeed without pointer access. A short source retains the tail.
	if (!wvrt_slice_try_copy(empty_target, empty_source) ||
		!wvrt_slice_try_copy(destination, (wvrt_slice){input, 3, width}) ||
		output[0] != 0 || output[1] != INT32_MIN || output[2] != INT32_MAX || output[3] != 44) {
		return 2;
	}

	// Two adjacent, disjoint windows can refer to one allocation.
	if (!wvrt_slice_try_copy((wvrt_mut_slice){output + 2, 2, width}, (wvrt_slice){output, 2, width}) ||
		output[0] != 0 || output[1] != INT32_MIN || output[2] != 0 || output[3] != INT32_MIN) {
		return 3;
	}
	return 0;
}

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_SEED_LOCAL_TYPES_H
#define WOVE_SEED_LOCAL_TYPES_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t seed_device_t;

struct seed_stat {
	seed_device_t st_dev;
};

static int32_t seed_error;
#define SEED_OK 0
#define SEED_DEVICE_OK(device) ((device) == 42)

// Populate the original native record and publish a scalar error value.
static inline int32_t seed_stat(int32_t device, struct seed_stat *info) {
	info->st_dev = (seed_device_t)device;
	seed_error = SEED_OK;
	return 0;
}

typedef size_t seed_count_t;
#define SEED_READ_MODE 97

// Pass a hidden fixed command before the promoted signed tail value.
static inline int32_t seed_control(int32_t descriptor, int32_t command, ...) {
	va_list arguments;
	va_start(arguments, command);
	const int tail = va_arg(arguments, int);
	va_end(arguments);
	return command == SEED_READ_MODE ? descriptor + tail : -1;
}

// Populate two native descriptor elements through the checked typed view.
static inline int32_t seed_pair(int32_t output[2]) {
	output[0] = 34;
	output[1] = 8;
	return 0;
}

// Write only the requested prefix of the caller's byte view.
static inline int32_t seed_capture(unsigned char *output, seed_count_t count) {
	// Keep the final checked byte distinct from the untouched suffix.
	for (seed_count_t i = 0; i < count; ++i) {
		output[i] = 19;
	}
	return 0;
}

struct seed_stream {
	int32_t sum;
};
typedef struct seed_stream *seed_stream_t;

static struct seed_stream seed_state;
#define seed_output (&seed_state)

// Copy the supplied bytes into the fixture stream's running sum.
static inline seed_count_t seed_write(seed_stream_t stream, seed_count_t count,
	const unsigned char *input, seed_count_t width) {
	// A three-byte input must reach the fixture in its original order.
	if (width == 1 && count == 3 && input[0] == 11 && input[1] == 0 && input[2] == 24) {
		stream->sum = input[0] + input[2];
		return count;
	}
	return 0;
}

// Record the final byte without taking ownership of the stream.
static inline int32_t seed_append(int32_t byte, seed_stream_t stream) {
	stream->sum += byte;
	return byte;
}

// A destructor call would make an unexpected transfer visible to the fixture.
static inline int32_t seed_close(seed_stream_t stream) {
	stream->sum = 0;
	return 0;
}

// Check the same borrowed stream retained the complete sequence.
static inline int32_t seed_flush(seed_stream_t stream) {
	return stream->sum == 42 ? 0 : 1;
}

#endif

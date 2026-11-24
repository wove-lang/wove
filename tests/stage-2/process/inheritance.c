// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "process.h"

#undef NDEBUG
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <Availability.h>
#endif

extern char **environ;

static char *fixture;
static bool force_fifo;
static int observed_endpoints;

// Launch an unrelated process at the moment an endpoint first exists.
static void check_inheritance(int fd) {
	char number[32];
	const int length = snprintf(number, sizeof number, "%d", fd);
	assert(length > 0 && (size_t)length < sizeof number);
	char *argv[] = {fixture, number, NULL};
	pid_t child;
	assert(posix_spawn(&child, fixture, NULL, NULL, argv, environ) == 0);
	int status;
	assert(waitpid(child, &status, 0) == child);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	++observed_endpoints;
}

// Intercept plain pipes so reverting to pipe followed by fcntl fails the fixture.
int wove_test_pipe(int descriptors[2]) {
	const int result = pipe(descriptors);
	if (result == 0) {
		check_inheritance(descriptors[0]);
		check_inheritance(descriptors[1]);
	}
	return result;
}

// Observe both endpoints at creation, or simulate a kernel without pipe2.
int wove_test_pipe2(int descriptors[2], int flags) {
	int result = -1;
	errno = ENOSYS;
	if (!force_fifo) {
#if defined(__linux__)
		result = pipe2(descriptors, flags);
#elif defined(__APPLE__) && defined(__MAC_27_0) && defined(__clang__)
		if (__builtin_available(macOS 27, *)) { result = pipe2(descriptors, flags); }
#else
		(void)flags;
#endif
	}
	if (result == 0) {
		check_inheritance(descriptors[0]);
		check_inheritance(descriptors[1]);
	}
	return result;
}

// The FIFO fallback opens one endpoint at a time with no mode argument.
int wove_test_open(const char *path, int flags, ...) {
	const int fd = open(path, flags);
	if (fd >= 0) { check_inheritance(fd); }
	return fd;
}

// Verify a capture still completes normally while the interleaved launches run.
static void check_capture(void) {
	const uint8_t tool[] = "/usr/bin/printf";
	const uint8_t arguments[] = "ok";
	uint8_t output[2];
	uint8_t result[5];
	const int previous = observed_endpoints;
	assert(wove_process_capture(tool, sizeof tool - 1, arguments, sizeof arguments,
		output, sizeof output, result, sizeof result) == 0);
	assert(observed_endpoints == previous + 2);
	assert(output[0] == 'o' && output[1] == 'k' && result[0] == 2 && result[4] == 0);
}

// A child checks the descriptor number it would have inherited from the capture.
int main(int argc, char **argv) {
	if (argc == 2) {
		errno = 0;
		return fcntl(atoi(argv[1]), F_GETFD) == -1 && errno == EBADF ? 0 : 1;
	}
	fixture = argv[0];
	check_capture();
	force_fifo = true;
	check_capture();
	return 0;
}

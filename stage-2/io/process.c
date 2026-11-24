// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#elif defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include "process.h"

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <Availability.h>
#endif

extern char **environ;

// Copy the program and packed arguments into one allocation with a terminated argv.
static char **make_argv(const uint8_t *program, size_t program_length,
	const uint8_t *arguments, size_t arguments_length) {
	if (memchr(program, 0, program_length) != NULL) { return NULL; }
	if (arguments_length != 0 && arguments[arguments_length - 1] != 0) { return NULL; }

	const size_t program_size = program_length + 1;
	if (arguments_length > SIZE_MAX - program_size) { return NULL; }
	const size_t text_size = program_size + arguments_length;
	size_t slots = 2;
	// Count the executable, each user argument including empty ones, and the sentinel.
	for (size_t index = 0; index < arguments_length; ++index) {
		slots += arguments[index] == 0;
	}
	if (slots > (SIZE_MAX - text_size) / sizeof(char *)) { return NULL; }
	char **argv = malloc(slots * sizeof *argv + text_size);
	if (argv == NULL) { return NULL; }
	char *text = (char *)(argv + slots);
	memcpy(text, program, program_length);
	text[program_length] = '\0';
	argv[0] = text;
	// A separator ends one argument and starts the next unless it ends the view.
	if (arguments_length != 0) {
		char *packed = text + program_size;
		memcpy(packed, arguments, arguments_length);
		size_t row = 1;
		argv[row++] = packed;
		// Stop before the final terminator, which does not start another argument.
		for (size_t index = 0; index + 1 < arguments_length; ++index) {
			if (packed[index] == '\0') { argv[row++] = packed + index + 1; }
		}
	}
	argv[slots - 1] = NULL;
	return argv;
}

// Open a private FIFO with close-on-exec already set, then remove its filesystem names.
static bool open_capture_fifo(int descriptors[2]) {
	const char *root = getenv("TMPDIR");
	if (root == NULL || root[0] == '\0') { root = "/tmp"; }
	static const char directory[] = "/wove-capture.XXXXXX";
	static const char name[] = "/stdout";
	const size_t length = strlen(root);
	if (length > SIZE_MAX - sizeof directory - sizeof name) { return false; }
	char *path = malloc(length + sizeof directory - 1 + sizeof name);
	if (path == NULL) { return false; }
	memcpy(path, root, length);
	memcpy(path + length, directory, sizeof directory);
	// A private directory keeps both FIFO opens tied to this capture.
	if (mkdtemp(path) == NULL) {
		free(path);
		return false;
	}
	char *tail = path + length + sizeof directory - 1;
	memcpy(tail, name, sizeof name);
	int reader = -1;
	int writer = -1;
	bool unlinked = false;
	// Open the reader first, temporarily nonblocking, so the writer can attach.
	if (mkfifo(path, S_IRUSR | S_IWUSR) == 0) {
		reader = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (reader >= 0) { writer = open(path, O_WRONLY | O_CLOEXEC); }
		unlinked = unlink(path) == 0;
	}
	*tail = '\0';
	const bool removed = rmdir(path) == 0;
	free(path);
	// After both opens, restore blocking reads without changing descriptor flags.
	if (writer >= 0 && unlinked && removed && fcntl(reader, F_SETFL, 0) == 0) {
		descriptors[0] = reader;
		descriptors[1] = writer;
		return true;
	}
	if (writer >= 0) { close(writer); }
	if (reader >= 0) { close(reader); }
	return false;
}

// Create close-on-exec descriptors without a separate file-descriptor flag update.
static bool create_capture_pipe(int descriptors[2]) {
#if defined(__linux__)
	if (pipe2(descriptors, O_CLOEXEC) == 0) { return true; }
	if (errno != ENOSYS) { return false; }
#elif defined(__APPLE__) && defined(__MAC_27_0) && defined(__clang__)
	// A newer SDK can still target a macOS release without pipe2.
	if (__builtin_available(macOS 27, *)) {
		if (pipe2(descriptors, O_CLOEXEC) == 0) { return true; }
		if (errno != ENOSYS) { return false; }
	}
#endif
	return open_capture_fifo(descriptors);
}

// Put both endpoints above stdio so the child can always duplicate then close them.
static bool open_capture_pipe(int descriptors[2]) {
	if (!create_capture_pipe(descriptors)) { return false; }
	size_t index = 0;
	// Relocation also creates close-on-exec descriptors atomically.
	for (; index < 2; ++index) {
		const int endpoint = descriptors[index];
		if (endpoint > STDERR_FILENO) { continue; }
		const int replacement = fcntl(endpoint, F_DUPFD_CLOEXEC, STDERR_FILENO + 1);
		if (replacement < 0) { break; }
		descriptors[index] = replacement;
		if (close(endpoint) != 0) { break; }
	}
	if (index == 2) { return true; }
	close(descriptors[0]);
	close(descriptors[1]);
	return false;
}

// Wire the child's stdout to the pipe and leave its other standard streams inherited.
static bool spawn_capture(pid_t *child, char **argv, const int descriptors[2]) {
	posix_spawn_file_actions_t actions;
	if (posix_spawn_file_actions_init(&actions) != 0) { return false; }
	const bool started =
		posix_spawn_file_actions_adddup2(&actions, descriptors[1], STDOUT_FILENO) == 0 &&
		posix_spawn_file_actions_addclose(&actions, descriptors[0]) == 0 &&
		posix_spawn_file_actions_addclose(&actions, descriptors[1]) == 0 &&
		posix_spawnp(child, argv[0], &actions, NULL, argv, environ) == 0;
	posix_spawn_file_actions_destroy(&actions);
	return started;
}

// Fill the prefix, then drain any extra bytes through a fixed stack buffer until EOF.
static bool drain_capture(int fd, uint8_t *output, int32_t capacity, int32_t *length, bool *overflow) {
	uint8_t discard[4096];
	// Each iteration targets only the unused part of the prefix or the discard buffer.
	for (;;) {
		const bool has_room = *length < capacity;
		const ssize_t count = read(fd, has_room ? output + *length : discard,
			has_room ? (size_t)(capacity - *length) : sizeof discard);
		// An interrupted read leaves the destination and prefix length unchanged.
		if (count < 0) {
			if (errno == EINTR) { continue; }
			return false;
		}
		if (count == 0) { return true; }
		if (has_room) { *length += (int32_t)count; }
		else { *overflow = true; }
	}
}

// Reap this child and translate its status to the archived Wove convention.
static int32_t wait_for_capture(pid_t child) {
	int status;
	pid_t finished;
	// Signals can interrupt waitpid before it has collected the child.
	do {
		finished = waitpid(child, &status, 0);
	} while (finished < 0 && errno == EINTR);
	if (finished < 0) { return -1; }
	if (WIFEXITED(status)) { return WEXITSTATUS(status); }
	if (WIFSIGNALED(status)) { return 128 + WTERMSIG(status); }
	return -1;
}

// Write the result in a fixed, alignment-independent layout for the byte-slice bridge.
static void write_capture_result(uint8_t *result, int32_t length, bool overflow) {
	const uint32_t width = (uint32_t)length;
	result[0] = (uint8_t)width;
	result[1] = (uint8_t)(width >> 8);
	result[2] = (uint8_t)(width >> 16);
	result[3] = (uint8_t)(width >> 24);
	result[4] = overflow;
}

// Validate and start one tool, then close both parent endpoints and reap it.
int32_t wove_process_capture(const uint8_t *program, int32_t program_length,
	const uint8_t *arguments, int32_t arguments_length, uint8_t *output, int32_t capacity,
	uint8_t *result, int32_t result_length) {
	if (result_length < 5) { return -1; }
	write_capture_result(result, 0, false);
	if (program_length <= 0 || arguments_length < 0 || capacity < 0) { return -1; }
	char **argv = make_argv(program, (size_t)program_length, arguments, (size_t)arguments_length);
	if (argv == NULL) { return -1; }

	int descriptors[2];
	// Pipe setup failures leave only the argument allocation to release.
	if (!open_capture_pipe(descriptors)) {
		free(argv);
		return -1;
	}
	pid_t child;
	const bool started = spawn_capture(&child, argv, descriptors);
	const bool output_closed = close(descriptors[1]) == 0;
	free(argv);
	// A failed spawn owns no child. The parent still owns the pipe reader.
	if (!started) {
		close(descriptors[0]);
		return -1;
	}

	int32_t length = 0;
	bool overflow = false;
	const bool readable = output_closed && drain_capture(descriptors[0], output, capacity, &length, &overflow);
	const bool input_closed = close(descriptors[0]) == 0;
	const int32_t status = wait_for_capture(child);
	write_capture_result(result, length, overflow);
	return readable && input_closed ? status : -1;
}

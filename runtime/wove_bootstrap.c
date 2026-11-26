// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "wove_runtime.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// Convert a checked byte view to a temporary POSIX path.
static char *wvrt_bootstrap_path_text(wvrt_slice path) {
	if (path.wvrt_length <= 0 || path.wvrt_element_size != 1 || path.wvrt_items == NULL ||
		memchr(path.wvrt_items, 0, (size_t)path.wvrt_length) != NULL) { return NULL; }
	char *text = malloc((size_t)path.wvrt_length + 1);
	if (text == NULL) { return NULL; }
	memcpy(text, path.wvrt_items, (size_t)path.wvrt_length);
	text[path.wvrt_length] = '\0';
	return text;
}

// Accept empty views for measurement calls without requiring backing storage.
static bool wvrt_bootstrap_byte_output(wvrt_mut_slice out) {
	return out.wvrt_element_size == 1 && out.wvrt_length >= 0 &&
		(out.wvrt_length == 0 || out.wvrt_items != NULL);
}

// Canonicalize a path and copy only when the caller can receive the complete spelling.
int32_t wvrt_bootstrap_canonical_path(wvrt_slice path, wvrt_mut_slice out) {
	if (!wvrt_bootstrap_byte_output(out)) { return -1; }
	char *text = wvrt_bootstrap_path_text(path);
	if (text == NULL) { return -1; }
	char *resolved = realpath(text, NULL);
	free(text);
	if (resolved == NULL) { return -1; }
	const size_t length = strlen(resolved);
	if (length <= (size_t)out.wvrt_length) { memcpy(out.wvrt_items, resolved, length); }
	free(resolved);
	return length <= INT32_MAX ? (int32_t)length : -1;
}

// Retry an interrupted POSIX read before reporting progress to the snapshot operation.
static ssize_t wvrt_bootstrap_read(int fd, void *out, size_t size) {
	ssize_t got;
	// EINTR consumes no input and leaves the destination unchanged.
	do {
		got = read(fd, out, size);
	} while (got < 0 && errno == EINTR);
	return got;
}

// Measure or read one already-open regular file within the caller's capacity.
static int32_t wvrt_bootstrap_read_open(int fd, wvrt_mut_slice out) {
	struct stat status;
	if (fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) ||
		status.st_size < 0 || status.st_size >= INT32_MAX) { return -1; }
	const int32_t length = (int32_t)status.st_size;
	if (length > out.wvrt_length) { return length; }
	size_t offset = 0;
	// Consume the measured extent and require EOF before accepting the snapshot.
	while (offset < (size_t)length) {
		const ssize_t got = wvrt_bootstrap_read(fd, (uint8_t *)out.wvrt_items + offset, (size_t)length - offset);
		if (got <= 0) { return -1; }
		offset += (size_t)got;
	}
	uint8_t extra;
	return wvrt_bootstrap_read(fd, &extra, 1) == 0 ? length : -1;
}

// Read a regular file without exporting its descriptor or any C-owned byte buffer.
int32_t wvrt_bootstrap_file(wvrt_slice path, wvrt_mut_slice out) {
	if (!wvrt_bootstrap_byte_output(out)) { return -1; }
	char *text = wvrt_bootstrap_path_text(path);
	if (text == NULL) { return -1; }
	const int fd = open(text, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
	free(text);
	if (fd < 0) { return -1; }
	const int32_t result = wvrt_bootstrap_read_open(fd, out);
	return close(fd) == 0 ? result : -1;
}

// Read a regular file's device and inode without leaking the host stat layout to the seed.
static bool wvrt_bootstrap_file_key(wvrt_slice path, struct stat *status) {
	char *text = wvrt_bootstrap_path_text(path);
	if (text == NULL) { return false; }
	const int result = stat(text, status);
	free(text);
	return result == 0 && S_ISREG(status->st_mode);
}

// Compare two disk names by identity, including hard links and symbolic aliases.
bool wvrt_bootstrap_same_file(wvrt_slice left, wvrt_slice right) {
	struct stat a;
	struct stat b;
	return wvrt_bootstrap_file_key(left, &a) && wvrt_bootstrap_file_key(right, &b) &&
		a.st_dev == b.st_dev && a.st_ino == b.st_ino;
}

// Write one complete byte view to the requested standard stream.
bool wvrt_bootstrap_write(wvrt_slice bytes, bool diagnostic) {
	if (bytes.wvrt_length < 0 || bytes.wvrt_element_size != 1 ||
		(bytes.wvrt_length > 0 && bytes.wvrt_items == NULL)) { return false; }
	FILE *stream = diagnostic ? stderr : stdout;
	const size_t length = (size_t)bytes.wvrt_length;
	return (length == 0 || fwrite(bytes.wvrt_items, 1, length, stream) == length) && fflush(stream) == 0;
}

// Return the host's native OS code for Wove source selection.
uint8_t wvrt_bootstrap_os(void) {
#if defined(__linux__)
	return 1;
#elif defined(__APPLE__)
	return 2;
#else
	return 0;
#endif
}

// Return the host's native architecture code for Wove source selection.
uint8_t wvrt_bootstrap_arch(void) {
#if defined(__aarch64__) || defined(__arm64__)
	return 1;
#elif defined(__x86_64__)
	return 2;
#else
	return 0;
#endif
}

// Preserve the bootstrap C compiler's deployment when comparing self-host emissions.
int32_t wvrt_bootstrap_deployment(void) {
#if defined(__APPLE__) && defined(__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__)
	const int32_t deployment = __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__;
	// Clang collapses the minor and patch components to one digit before macOS 10.10.
	if (deployment < 100000) {
		return (deployment / 100) * 10000 + (deployment / 10 % 10) * 100 + deployment % 10;
	}
	return deployment;
#else
	return -1;
#endif
}

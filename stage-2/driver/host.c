// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include "host.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char project_file[] = "wovec.jsonc";

// Size a regular input for the signed Wove slice ABI. Absence has its own status.
int32_t wove_cli_input_size(const char *path) {
	struct stat info;
	// File and parent lookup errors other than absence are hard failures.
	if (stat(path, &info) != 0) {
		return errno == ENOENT ? -1 : -2;
	}
	return S_ISREG(info.st_mode) && info.st_size >= 0 && info.st_size < INT32_MAX
		? (int32_t)info.st_size : -2;
}

// Read the measured byte count, reject trailing data, and close the stream.
static int32_t read_exact(FILE *file, uint8_t *bytes, int32_t length) {
	const bool complete = fread(bytes, 1, (size_t)length, file) == (size_t)length &&
		fgetc(file) == EOF && !ferror(file);
	const bool closed = fclose(file) == 0;
	return complete && closed ? 0 : -1;
}

// Fill the caller's exact-length view and reject a file whose length changed.
int32_t wove_cli_read_input(const char *path, uint8_t *bytes, int32_t length) {
	FILE *file = fopen(path, "rb");
	return file == NULL ? -1 : read_exact(file, bytes, length);
}

// Resolve a literal path against the defining module's directory using host path rules.
static char *asset_path(const char *module, const char *request) {
	if (request[0] == '/') { return strdup(request); }
	const char *slash = strrchr(module, '/');
	const size_t directory = slash == NULL ? 0 : (size_t)(slash - module) + 1;
	const size_t width = strlen(request);
	if (width >= SIZE_MAX - directory) { return NULL; }
	char *path = malloc(directory + width + 1);
	if (path == NULL) { return NULL; }
	memcpy(path, module, directory);
	memcpy(path + directory, request, width + 1);
	return path;
}

// Query the raw asset's size without requiring it to be a Wove source file.
int32_t wove_cli_asset_size(const char *module, const char *request) {
	char *path = asset_path(module, request);
	if (path == NULL) { return -1; }
	const int32_t size = wove_cli_input_size(path);
	free(path);
	return size;
}

// Read the measured regular file into its bounded region in the compilation arena.
int32_t wove_cli_read_asset(const char *module, const char *request, uint8_t *bytes,
	int32_t length, int32_t start, int32_t end) {
	if (start < 0 || end < start || end > length) { return -1; }
	char *path = asset_path(module, request);
	if (path == NULL) { return -1; }
	const int descriptor = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	free(path);
	if (descriptor < 0) { return -1; }
	struct stat info;
	// A replaced path must still be a regular file of the measured extent.
	if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size != end - start) {
		close(descriptor);
		return -1;
	}
	FILE *file = fdopen(descriptor, "rb");
	// On failure, fdopen leaves the original descriptor owned by this function.
	if (file == NULL) {
		close(descriptor);
		return -1;
	}
	return read_exact(file, bytes + start, end - start);
}

// Locate the project configuration in the current project directory.
int32_t wove_cli_project_size(void) {
	return wove_cli_input_size(project_file);
}

// Read project bytes through the same bounded host operation as source.
int32_t wove_cli_read_project(uint8_t *bytes, int32_t length) {
	return wove_cli_read_input(project_file, bytes, length);
}

// Accept exactly one directory component for a profile or target basename.
static bool component(const char *name) {
	return name[0] != '\0' && strchr(name, '/') == NULL &&
		strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

// Locate the final component using the same spelling as retained path construction.
static const char *target_basename(const char *target) {
	const char *slash = strrchr(target, '/');
	return slash == NULL ? target : slash + 1;
}

// Check the output operand before compilation can open the source.
bool wove_cli_valid_target(const char *target) {
	return component(target_basename(target));
}

// Reuse an existing directory or create a missing component.
static bool ensure_directory(const char *path) {
	// A successful mkdir establishes the component directly.
	if (mkdir(path, 0777) == 0) { return true; }
	struct stat info;
	return errno == EEXIST && stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

// Create each missing directory in the retained path before writing its artifact.
static bool ensure_directories(char *path) {
	// A leading slash is the filesystem root, which already exists.
	for (char *cursor = path + (path[0] == '/'); *cursor != '\0'; ++cursor) {
		// Repeated separators name the same directory.
		if (*cursor == '/' && cursor > path && cursor[-1] != '/') {
			*cursor = '\0';
			const bool ready = ensure_directory(path);
			*cursor = '/';
			if (!ready) { return false; }
		}
	}
	return ensure_directory(path);
}

// Protect the original source and project even when the retained name is an alias.
static bool is_same_file(const char *left, const char *right) {
	struct stat first, second;
	return stat(left, &first) == 0 && stat(right, &second) == 0 &&
		first.st_dev == second.st_dev && first.st_ino == second.st_ino;
}

// Publish a complete file using a temporary sibling in the same directory.
static bool write_unit(const char *path, const uint8_t *bytes, int32_t length) {
	const size_t width = strlen(path);
	char *temporary = malloc(width + sizeof ".XXXXXX");
	if (temporary == NULL) { return false; }
	memcpy(temporary, path, width);
	memcpy(temporary + width, ".XXXXXX", sizeof ".XXXXXX");
	const int descriptor = mkstemp(temporary);
	bool published = false;
	// Only a fully written and closed sibling can replace the retained unit.
	if (descriptor >= 0) {
		FILE *file = fdopen(descriptor, "wb");
		if (file == NULL) {
			close(descriptor);
		} else {
			const bool written = fwrite(bytes, 1, (size_t)length, file) == (size_t)length;
			const bool closed = fclose(file) == 0;
			published = written && closed && rename(temporary, path) == 0;
		}
		if (!published) { unlink(temporary); }
	}
	free(temporary);
	return published;
}

// Publish one C unit atomically in the archive's retained target directory.
int32_t wove_cli_publish(const char *target, const char *profile, uint32_t mode, const char *input,
		const uint8_t *bytes, int32_t length) {
	const char *name = target_basename(target);
	const char *parent = name == target ? "." : target;
	const int prefix = name == target ? 1 : (int)(name - target - 1);
	// Built-in modes use their archive directory. A named profile uses its own component.
	if (profile[0] == '\0' || strcmp(profile, "default") == 0) {
		profile = mode == 1 ? "debug" : mode == 2 ? "release" : "default";
	}
	if (!component(name) || !component(profile)) { return -1; }
	const int size = snprintf(NULL, 0, "%.*s/.wovec-cache/%s/%s/bin", prefix, parent, profile, name);
	if (size < 0) { return -1; }
	char *path = malloc((size_t)size + sizeof "/main.c");
	if (path == NULL) { return -1; }
	snprintf(path, (size_t)size + 1, "%.*s/.wovec-cache/%s/%s/bin", prefix, parent, profile, name);
	bool published = false;
	// No source or project file may also be the retained output.
	if (ensure_directories(path)) {
		memcpy(path + size, "/main.c", sizeof "/main.c");
		published = !is_same_file(path, input) && !is_same_file(path, project_file) && write_unit(path, bytes, length);
	}
	free(path);
	return published ? 0 : -1;
}

// Write the Wove serializer's original JSON bytes as one stderr record.
int32_t wove_cli_stderr(const uint8_t *bytes, int32_t length) {
	const bool written = fwrite(bytes, 1, (size_t)length, stderr) == (size_t)length;
	return written && fputc('\n', stderr) != EOF ? 0 : -1;
}

// Copy fixed diagnostic text or the project path into a caller-owned byte view.
int32_t wove_cli_text(uint32_t text, uint8_t *bytes, int32_t capacity) {
	static const char *const messages[] = {
		project_file, "invalid command arguments", "invalid project configuration",
		"invalid Wove source", "could not read file", "could not publish retained C",
		"cc requires one explicit source and an explicit output target",
		"source operand must be UTF-8", "invalid output target", "could not read embedded asset"
	};
	// The supplied view must fit the whole string without its terminator.
	if (text >= sizeof messages / sizeof *messages) { return -1; }
	const size_t length = strlen(messages[text]);
	if (length > (size_t)capacity) { return -1; }
	memcpy(bytes, messages[text], length);
	return (int32_t)length;
}

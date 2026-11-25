// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include "host.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char project_file[] = "wovec.jsonc";

// Accept a single named directory component.
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
static bool ensure_parent_directories(char *path) {
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
	return true;
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

// Publish atomically at the supplied destination after protecting the source and project files.
int32_t wove_cli_publish(const char *path, const char *input, const uint8_t *bytes, int32_t length) {
	char *parents = strdup(path);
	if (parents == NULL) { return -1; }
	const bool ready = ensure_parent_directories(parents);
	free(parents);
	const bool published = ready && !is_same_file(path, input) && !is_same_file(path, project_file) &&
		write_unit(path, bytes, length);
	return published ? 0 : -1;
}

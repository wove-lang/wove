// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#undef main
int wove_fixture_main(int argc, char **argv);

// Verify the returned descriptor before releasing it.
static bool verify_descriptor(int fd, mode_t permissions) {
	if (fd < 0) { return false; }
	struct stat info;
	bool valid = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
		(info.st_mode & 0777) == permissions;
	return close(fd) == 0 && valid;
}

// Isolate the real file creation calls in a directory beside the test executable.
int main(int argc, char **argv) {
	(void)argc;
	char directory[PATH_MAX];
	int length = snprintf(directory, sizeof directory, "%s.files.XXXXXX", argv[0]);
	if (length < 0 || (size_t)length >= sizeof directory) { return 1; }
	int previous = open(".", O_RDONLY);
	if (previous < 0) { return 2; }
	if (mkdtemp(directory) == NULL) {
		(void)close(previous);
		return 3;
	}

	int result = 4;
	// Run both paths with a known creation mask and restore the working directory for cleanup.
	if (chdir(directory) == 0) {
		mode_t saved = umask(0);
		char *with_open[] = {argv[0], "first", NULL};
		char *with_openat[] = {argv[0], "second", "relative", NULL};
		bool first = verify_descriptor(wove_fixture_main(2, with_open), 0600);
		bool second = verify_descriptor(wove_fixture_main(3, with_openat), 0640);
		(void)umask(saved);
		int removed_first = unlink("first");
		int removed_second = unlink("second");
		if (fchdir(previous) == 0 && first && second && removed_first == 0 && removed_second == 0) {
			result = 42;
		}
	}
	if (close(previous) != 0) { result = 5; }
	if (rmdir(directory) != 0) { result = 6; }
	return result;
}

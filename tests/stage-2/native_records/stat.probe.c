// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <fcntl.h>
#include <unistd.h>

#undef main

int wove_fixture_main(int argc, char **argv);

// Attach the named fixture to stdin so Wove can compare path and descriptor metadata.
int main(void) {
	char path[] = "tests/stage-2/native_records/data.txt";
	int fd = open(path, O_RDONLY);
	if (fd < 0) { return 5; }
	// Preserve descriptor zero if open already used it.
	if (fd != STDIN_FILENO) {
		if (dup2(fd, STDIN_FILENO) < 0) { return 6; }
		if (close(fd) != 0) { return 7; }
	}
	char *argv[] = {path, path, 0};
	return wove_fixture_main(2, argv);
}

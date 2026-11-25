// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#undef main

int wove_fixture_main(int argc, char **argv);

// Confirm that returning from Wove leaves the C library's stderr open.
int main(int argc, char **argv) {
	FILE *stream = stderr;
	int status = wove_fixture_main(argc, argv);
	// Check the original descriptor before asking libc to write through the stream.
	if (status != 42 || stderr != stream || fcntl(STDERR_FILENO, F_GETFD) == -1) {
		return status == 42 ? 8 : status;
	}
	return fputc('!', stderr) == '!' ? 42 : 9;
}

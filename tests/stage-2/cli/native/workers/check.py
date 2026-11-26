# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


# Run native cases and force both thread API failures in a separate runtime object.
def main():
	directory = Path(__file__).resolve().parent
	compiler = Path(sys.argv[1]).resolve()
	subprocess.run([sys.executable, directory.parent / "check.py", compiler, directory / "cases.tsv"], check=True)
	runtime = Path("runtime").resolve()
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	flags = shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))
	command = [*clang, "-std=c11", *flags, "-pthread", "-I", runtime]
	# Rename only the runtime's thread calls so the fixture can still call the actual POSIX APIs.
	with tempfile.TemporaryDirectory(prefix="joined-worker-") as work:
		output = Path(work)
		object_file = output / "runtime.o"
		program = output / "failure"
		subprocess.run([*command, "-Dpthread_create=fixture_worker_create", "-Dpthread_join=fixture_worker_join",
			"-c", runtime / "wove_runtime.c", "-o", object_file], check=True)
		subprocess.run([*command, directory / "failure.c", object_file, "-o", program], check=True)
		# Verify both the reported span and the exit status for each runtime path.
		for mode, status, error in (
			("success", 0, ""),
			("create", 1, "file 2 bytes [10,37): runtime error: could not start joined worker\n"),
			("join", 1, "file 2 bytes [10,37): runtime error: could not join worker\n"),
		):
			result = subprocess.run([program, mode], capture_output=True, text=True, timeout=10)
			assert (result.returncode, result.stdout, result.stderr) == (status, "", error), result


if __name__ == "__main__":
	main()

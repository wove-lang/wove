# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Run native CLI fixtures against the macro inventory of the configured C header."""

from functools import cache
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory

from check import check_case, manifest_rows


# Query the same hosted frontend for each header and C mode used by the manifest.
@cache
def macro_inventory(frontend, standard, header, flags=()):
	capture = subprocess.check_output(
		[frontend, f"-std={standard}", *flags, "-dM", "-E", "-x", "c", "-include", header, "-"],
		input=b"", timeout=30)
	# Function-like definitions have an opening parenthesis directly after the name.
	return frozenset(fields[1] for line in capture.splitlines()
		if len(fields := line.split(None, 2)) == 3 and fields[0] == b"#define")


# Reuse ordinary publication, diagnostic, and execution expectations for matching cases.
def main():
	compiler, manifest = (Path(value).resolve() for value in sys.argv[1:])
	frontend = os.environ.get("CLANG", "clang")
	clang = [*shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang")), "-std=c11",
		*shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))]
	runtime = Path(os.environ.get("WOVE_TEST_RUNTIME", "runtime/wove_runtime.c")).resolve()
	with TemporaryDirectory(prefix="native-macro-modes-") as install:
		installed = Path(install) / compiler.name
		shutil.copy2(compiler, installed)
		for standard, header, name, required, *row in manifest_rows(manifest):
			present = name.encode() in macro_inventory(frontend, standard, header)
			if present == (required == "present"):
				check_case(installed, clang, runtime, manifest.parent, row)


if __name__ == "__main__":
	main()

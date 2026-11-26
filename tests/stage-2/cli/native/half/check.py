# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
from pathlib import Path
import shlex
import subprocess
import sys


# Run the rejection cases on every host. The native ABI cases need Clang binary16.
def main():
	directory = Path(__file__).resolve().parent
	compiler = Path(sys.argv[1]).resolve()
	check = [sys.executable, str(directory.parent / "check.py"), str(compiler)]
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	env = {**os.environ, "CLANG": clang[0]}
	subprocess.run([*check, directory / "unavailable.tsv"], env=env, check=True)
	macros = subprocess.check_output([*clang, "-dM", "-E", "-x", "c", "-"], input=b"")
	# The backend and runtime use this same target fact.
	if b"#define __FLT16_MANT_DIG__ 11" in macros.splitlines():
		subprocess.run([*check, directory / "supported.tsv"], env=env, check=True)


if __name__ == "__main__":
	main()

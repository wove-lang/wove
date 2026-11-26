# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
from pathlib import Path
import shlex
import subprocess
import sys


# Run rejection cases everywhere and execute supported conversions on the configured native target.
def main():
	directory = Path(__file__).resolve().parent
	compiler = Path(sys.argv[1]).resolve()
	check = [sys.executable, str(directory.parent / "check.py"), str(compiler)]
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	env = {**os.environ, "CLANG": clang[0]}
	subprocess.run([*check, directory / "unavailable.tsv"], env=env, check=True)
	macros = subprocess.check_output([*clang, "-dM", "-E", "-x", "c", "-"], input=b"").splitlines()
	# The native fixture needs the same long-double mantissa the compiler probes.
	if b"#define __LDBL_MANT_DIG__ 64" not in macros:
		return
	subprocess.run([*check, directory / "supported.tsv"], env=env, check=True)
	# Cross-format cases also require the independently probed binary16 ABI.
	if b"#define __FLT16_MANT_DIG__ 11" in macros:
		subprocess.run([*check, directory / "half.tsv"], env=env, check=True)


if __name__ == "__main__":
	main()

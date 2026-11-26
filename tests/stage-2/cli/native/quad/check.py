# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
from pathlib import Path
import shlex
import subprocess
import sys


# Run rejection cases everywhere and execute bridge cases on a native binary128 target.
def main():
	directory = Path(__file__).resolve().parent
	compiler = Path(sys.argv[1]).resolve()
	check = [sys.executable, str(directory.parent / "check.py"), str(compiler)]
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	env = {**os.environ, "CLANG": clang[0]}
	subprocess.run([*check, directory / "unavailable.tsv"], env=env, check=True)
	macros = subprocess.check_output([*clang, "-dM", "-E", "-x", "c", "-"], input=b"").splitlines()
	long_double_quad = b"#define __LDBL_MANT_DIG__ 113" in macros
	extension_quad = any(line.startswith(b"#define __SIZEOF_FLOAT128__ ") for line in macros)
	# Either prototype spelling can provide the target ABI.
	if not (long_double_quad or extension_quad):
		return
	subprocess.run([*check, directory / "supported.tsv"], env=env, check=True)
	# Run the narrower format combinations when the target supports each ABI.
	if b"#define __LDBL_MANT_DIG__ 64" in macros:
		subprocess.run([*check, directory / "extended.tsv"], env=env, check=True)
	if b"#define __FLT16_MANT_DIG__ 11" in macros:
		subprocess.run([*check, directory / "half.tsv"], env=env, check=True)


if __name__ == "__main__":
	main()

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
from pathlib import Path
import shlex
import subprocess
import sys


# Check diagnostics on every host and execute values on a native int128 target.
def main():
	directory = Path(__file__).resolve().parent
	compiler = Path(sys.argv[1]).resolve()
	check = [sys.executable, str(directory.parent / "check.py"), str(compiler)]
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	subprocess.run([*check, directory / "unavailable.tsv"], check=True)
	macros = subprocess.check_output([*clang, "-dM", "-E", "-x", "c", "-"], input=b"").splitlines()
	# A native executable needs the same capability as the Clang probe.
	if any(line.startswith(b"#define __SIZEOF_INT128__ ") for line in macros):
		subprocess.run([*check, directory / "supported.tsv"], check=True)
		# Mixed optional formats require both target facts before a value fixture can run.
		if b"#define __FLT16_MANT_DIG__ 11" in macros:
			subprocess.run([*check, directory / "float-half.tsv"], check=True)
		if b"#define __LDBL_MANT_DIG__ 64" in macros:
			subprocess.run([*check, directory / "float-f80.tsv"], check=True)
		if b"#define __LDBL_MANT_DIG__ 113" in macros or any(
			line.startswith(b"#define __SIZEOF_FLOAT128__ ") for line in macros
		):
			subprocess.run([*check, directory / "float-f128.tsv"], check=True)


if __name__ == "__main__":
	main()

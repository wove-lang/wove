# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Check native record byte access and C ABI rejections."""

import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory


# Check the actual Clang diagnostic against the field's span in the imported provider.
def check_abi_mismatches(compiler, fixtures, env):
	with TemporaryDirectory(prefix="wove-inline-field-") as scratch:
		root = Path(scratch)
		for name in ("tag.wove", "provider.wove", "tag.h"):
			shutil.copyfile(fixtures / name, root / name)
		declaration = (root / "provider.wove").read_bytes()
		operand = b"(array char 17)"
		start = declaration.index(operand)
		end = start + len(operand)
		span = rb"Wove C inline field type or extent in file [1-9][0-9]* bytes \[%d,%d\)" % (start, end)
		for member in ("signed char name[17]", "unsigned char name[17]", "char name[16]"):
			command = [compiler, "bin", "tag.wove", "--no-human", "-o", "out",
				"--cflag=-I.", "--cflag=-DWOVE_TAG_NAME=" + member]
			result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=30)
			assert result.returncode == 1 and not (root / "out").exists(), (member, result)
			assert re.search(span, result.stderr), (member, result.stderr)


# Reuse the native CLI harness for the Wove fixture cases.
def main():
	fixtures = Path(__file__).resolve().parent
	compiler = Path(sys.argv[1]).resolve()
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))[0]
	env = {**os.environ, "CLANG": os.environ.get("CLANG", clang)}
	subprocess.run([sys.executable, fixtures.parent / "check.py", compiler, fixtures / "cases.tsv"],
		env=env, check=True)
	check_abi_mismatches(compiler, fixtures, env)


if __name__ == "__main__":
	main()

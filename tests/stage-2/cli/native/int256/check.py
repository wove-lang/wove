# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
from pathlib import Path
import shlex
import subprocess
import sys
from tempfile import TemporaryDirectory


# Check real toolchain support before choosing the native execution or rejection cases.
def main():
	directory = Path(__file__).resolve().parent
	compiler, wide_probe = (Path(value).resolve() for value in sys.argv[1:])
	check = [sys.executable, str(directory.parent / "check.py"), str(compiler)]
	selected = os.environ.get("CLANG")
	clang = [selected] if selected else shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	clang = [*clang, "-O1", "-std=c23"]
	subprocess.run([*check, directory / "unavailable.tsv"], check=True)
	# A control link distinguishes missing 256-bit support from an unavailable C23 toolchain.
	with TemporaryDirectory(prefix="int256-control-") as workspace:
		control = Path(workspace) / "control"
		ordinary = subprocess.run([*clang, directory / "control.c", "-o", control], capture_output=True, timeout=30)
		if ordinary.returncode:
			return
		probe = subprocess.run([*clang, wide_probe, "-o", Path(workspace) / "wide"], capture_output=True, timeout=30)
		# The control identifies whether this toolchain's linked images can run on the test host.
		try:
			native = subprocess.run([control], check=True, timeout=30).returncode == 0
		except (OSError, subprocess.CalledProcessError):
			native = False
	if probe.returncode:
		subprocess.run([*check, directory / "unsupported.tsv"], check=True)
	elif native:
		subprocess.run([*check, directory / "supported.tsv"], check=True)
	else:
		print("C23 _BitInt(256) native execution skipped: selected Clang targets another architecture.")


if __name__ == "__main__":
	main()

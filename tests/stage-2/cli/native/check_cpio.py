# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

from itertools import product
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory

from check import manifest_rows
from constant_result import folded_i32_result


# Link the relocated compiler's output and compare every native macro value.
def check_case(compiler, clang, runtime, fixtures, project, header, standard, flags):
	project.mkdir()
	# Both bridge origins read the same native header through distinct import keys.
	for name in ("project.wove", "oracle.h", "user.h"):
		shutil.copy2(fixtures / name, project / name)
	source = (fixtures / "probe.wove").read_text()
	(project / "main.wove").write_text(source.replace("CPIO_HEADER", header))
	flags = [*flags, "-I.", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
	result = subprocess.run([compiler, "bin", "main.wove", "-o", "out", f"--cstd={standard}",
		*(f"--cflag={flag}" for flag in flags)], cwd=project,
		env={**os.environ, "CLANG": clang}, capture_output=True, timeout=30)
	assert result.returncode == 0, (project.name, result.stderr)
	# Give the optimizer the existing borrow guards for these constant views.
	status = folded_i32_result(clang, runtime, project, standard,
		[*flags, "-include", str(runtime / "wove_runtime.c")])
	assert status == 42, (project.name, status)


# Run both fixture origins under all selected standard and feature profiles.
def main():
	compiler, runtime = (Path(value).resolve() for value in sys.argv[1:])
	fixtures = Path(__file__).resolve().parent / "cpio"
	clang = os.environ.get("CLANG", "clang")
	cases = list(manifest_rows(fixtures / "cases.tsv"))
	profiles = [(label, json.loads(flags)) for label, flags in manifest_rows(fixtures / "profiles.tsv")]
	# Move the compiler and every project outside the source checkout.
	with TemporaryDirectory(prefix="wove-cpio-") as directory:
		stage = Path(directory)
		installed = stage / compiler.name
		shutil.copy2(compiler, installed)
		# Hold each origin to the same C flags and shared native oracle.
		for (origin, header), standard, (profile, flags) in product(cases, ("c11", "c17", "c23"), profiles):
			check_case(installed, clang, runtime, fixtures, stage / f"{origin}-{standard}-{profile}",
				header, standard, flags)
	print(f"cpio.h: bundled and project macro values checked in {3 * len(profiles)} host profiles")


if __name__ == "__main__":
	main()

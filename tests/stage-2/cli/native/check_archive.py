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

from check import check_phase, manifest_rows
from check_macro_modes import macro_inventory
from constant_result import folded_i32_result


# Stage the shared fixture and give cc and bin the same C mode and feature flags.
def stage_project(fixtures, project, header, source, standard, flags):
	project.mkdir()
	# Both bridge origins read the same native header through distinct import keys.
	for name in ("project.wove", "oracle.h", "user.h"):
		shutil.copy2(fixtures / name, project / name)
	template = (fixtures / source).read_text()
	(project / "main.wove").write_text(template.replace("TEST_HEADER", header))
	config = {"profiles": [{"name": "default", "cc": {"cstd": standard},
		"build": {"default": {"cflags": flags}}}]}
	(project / "wovec.jsonc").write_text(json.dumps(config))


# Check the native values or the exact selector diagnostic for a hidden macro.
def check_case(compiler, clang, runtime, fixtures, project, header, standard, flags, *,
		source="probe.wove", unavailable=None):
	flags = [*flags, "-I.", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
	stage_project(fixtures, project, header, source, standard, flags)
	env = {**os.environ, "CLANG": clang}
	# Check-only matches the stored diagnostic when the SDK hides the selector.
	if unavailable is not None:
		result = subprocess.run([compiler, "cc", "--check", "main.wove"],
			cwd=project, env=env, capture_output=True, timeout=30)
		check_phase(project.name, "check", result, {"exit": 1}, fixtures, unavailable)
		return
	result = subprocess.run([compiler, "bin", "main.wove", "-o", "out"],
		cwd=project, env=env, capture_output=True, timeout=30)
	assert result.returncode == 0, (project.name, result.stderr)
	# Give the optimizer the existing borrow guards for these constant views.
	status = folded_i32_result(clang, runtime, project, standard,
		[*flags, "-include", str(runtime / "wove_runtime.c")])
	assert status == 42, (project.name, status)


# Run both fixture origins under all selected standard and feature profiles.
def main():
	compiler, runtime, fixtures = (Path(value).resolve() for value in sys.argv[1:])
	clang = os.environ.get("CLANG", "clang")
	cases = list(manifest_rows(fixtures / "cases.tsv"))
	optional = fixtures / "optional.tsv"
	optional_cases = list(manifest_rows(optional)) if optional.is_file() else []
	profiles = [(label, json.loads(flags))
		for label, flags in manifest_rows(fixtures.parent / "archive-profiles.tsv")]
	# Move the compiler and every project outside the source checkout.
	with TemporaryDirectory(prefix=f"wove-{fixtures.name}-") as directory:
		stage = Path(directory)
		installed = stage / compiler.name
		shutil.copy2(compiler, installed)
		# Hold each origin to the same C flags and shared native oracle.
		for (origin, header), standard, (profile, flags) in product(cases, ("c11", "c17", "c23"), profiles):
			project = stage / f"{origin}-{standard}-{profile}"
			check_case(installed, clang, runtime, fixtures, project, header, standard, flags)
			# Test optional declarations according to the configured compiler's header.
			for case_origin, native, macro, source, diagnostic in optional_cases:
				if case_origin != origin:
					continue
				present = macro.encode() in macro_inventory(clang, standard, native, tuple(flags))
				check_case(installed, clang, runtime, fixtures, project.with_name(f"{project.name}-{macro}"),
					header, standard, flags, source=source, unavailable=None if present else diagnostic)
	print(f"{fixtures.name}.h: bundled and project macros checked in {3 * len(profiles)} host profiles")


if __name__ == "__main__":
	main()

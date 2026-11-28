# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from constant_result import folded_i32_result


C_TYPES = {"i32": "int32_t", "u32": "uint32_t", "i64": "int64_t", "u64": "uint64_t"}
CLEAR_FEATURES = ("-U_GNU_SOURCE", "-U_DARWIN_C_SOURCE", "-U_DEFAULT_SOURCE",
	"-U_POSIX_C_SOURCE", "-U_XOPEN_SOURCE")
PROFILES = {
	"iso": (),
	"unsigned-char": ("-funsigned-char",),
	"posix": ("-D_POSIX_C_SOURCE=200809L",),
	"extensions": ("-D_GNU_SOURCE=1", "-D_DARWIN_C_SOURCE=1"),
}


# Read native macro presence under the same mode and feature flags as wovec.
def macro_names(clang, standard, flags):
	capture = subprocess.check_output(
		[clang, f"-std={standard}", *flags, "-E", "-dM", "-include", "limits.h", "-x", "c", "-"],
		input=b"", timeout=30)
	return {fields[1].decode("ascii") for line in capture.splitlines()
		if len(fields := line.split(None, 2)) == 3 and fields[0] == b"#define"}


# Repeat each templated line once per native declaration in manifest order.
def stage_probe(project, templates, members):
	fields = re.compile(r"LIMIT_[A-Z_]+")
	rows = [{"LIMIT_MEMBER": name, "LIMIT_ORACLE": "wove_oracle_" + name,
		"LIMIT_TYPE": kind, "LIMIT_C_TYPE": C_TYPES[kind], "LIMIT_STATUS": str(status)}
		for status, (name, kind) in enumerate(members, 64)]
	# The Wove and C templates share row names and leave all other lines intact.
	for target, template in templates:
		source = []
		# Expand only the lines marked with a per-member field.
		for line in template.splitlines(keepends=True):
			if fields.search(line):
				source.extend(fields.sub(lambda match: row[match[0]], line) for row in rows)
			else:
				source.append(line)
		(project / target).write_text("".join(source))


# Read native member and diagnostic cases from tab-separated fixture data.
def manifest_rows(path):
	return (line.split("\t") for line in path.read_text().splitlines()
		if line and not line.startswith("#"))


# Apply diagnostics only in the native profiles where the named member is absent.
def check_unavailable(compiler, project, env, native_names, cases):
	# Fixtures keep the operand and source span independent of the test runner.
	for macro, path, argv, status, expected in cases:
		if macro in native_names:
			continue
		shutil.copy2(path, project / path.name)
		result = subprocess.run([compiler, *argv], cwd=project, env=env,
			capture_output=True, timeout=30)
		assert result.returncode == status, (project.name, path.name, result)
		assert [json.loads(line) for line in result.stderr.splitlines()] == expected, (
			project.name, path.name, result.stderr)


# Report the manifest member if Clang folds the test to a mismatch.
def assert_constant_result(clang, runtime, project, standard, flags, selected):
	status = folded_i32_result(clang, runtime, project, standard, flags)
	# A constant failure stores the manifest position in the return value.
	if 64 <= status < 64 + len(selected):
		raise AssertionError((project.name, selected[status - 64][0], "value mismatch"))
	assert status == 42, (project.name, status)


# Verify live bindings and the applicable missing-declaration fixtures.
def check_profile(compiler, clang, runtime, project, templates, members, cases, standard, flags):
	project.mkdir()
	native_names = macro_names(clang, standard, flags)
	required = {name for name, _, availability in members if availability == "required"}
	assert required <= native_names, (project.name, required - native_names)
	selected = [(name, kind) for name, kind, _ in members if name in native_names]
	config = {"profiles": [{"name": "default", "cc": {"cstd": standard},
		"build": {"default": {"cflags": [*flags, "-I.", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]}}}]}
	(project / "wovec.jsonc").write_text(json.dumps(config))
	stage_probe(project, templates, selected)

	env = {**os.environ, "CLANG": clang}
	built = subprocess.run([compiler, "bin", "main.wove", "-o", "out"],
		cwd=project, env=env, capture_output=True, timeout=30)
	assert built.returncode == 0, (project.name, built.stderr)
	# Every typed comparison against the C oracle must fold to success.
	assert_constant_result(clang, runtime, project, standard, flags, selected)
	check_unavailable(compiler, project, env, native_names, cases)
	return {name for name, _ in selected}


# Run from a relocated compiler and a clean project for each host profile.
def main():
	fixtures = Path(__file__).resolve().parent
	compiler, runtime = (Path(value).resolve() for value in sys.argv[1:])
	clang = os.environ.get("CLANG", "clang")
	templates = [("main.wove", (fixtures / "probe.wove").read_text()),
		("oracle.h", (fixtures / "oracle.h").read_text())]
	members = [(name, kind, availability) for name, kind, availability in manifest_rows(fixtures / "members.tsv")
		if availability != "darwin" or sys.platform == "darwin"]
	cases = [(macro, fixtures / source, json.loads(argv), int(status), json.loads(expected))
		for macro, source, argv, status, expected in manifest_rows(fixtures / "unavailable.tsv")]
	covered = set()
	# The provider is embedded in the relocated executable.
	with TemporaryDirectory(prefix="wove-limits-") as directory:
		stage = Path(directory)
		installed = stage / compiler.name
		shutil.copy2(compiler, installed)
		# Exercise each C mode with standard, POSIX, and extension feature sets.
		for standard in ("c11", "c17", "c23"):
			# The unsigned-char profile checks that CHAR_MIN comes from the current header.
			for profile, flags in PROFILES.items():
				covered.update(check_profile(installed, clang, runtime, stage / f"{standard}-{profile}",
					templates, members, cases, standard, (*CLEAR_FEATURES, *flags)))
	print(f"limits.h: {len(covered)} live members checked across 12 host profiles")


if __name__ == "__main__":
	main()

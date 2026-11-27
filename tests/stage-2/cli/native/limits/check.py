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


# Pair each selected Wove value with a typed expression that reads the same native macro.
def stage_probe(project, template, members):
	oracle = ["#include <limits.h>", "#include <stdint.h>", ""]
	declarations = []
	oracles = []
	checks = []
	# Reserve result codes 64 and above to identify which comparison failed.
	for status, (name, kind) in enumerate(members, 64):
		oracle_name = "wove_oracle_" + name
		oracles.append(f"(value {oracle_name})")
		declarations.append(f"(let {oracle_name} (:pragma (c_macro)) {kind})")
		oracle.append(f"#define {oracle_name} (({C_TYPES[kind]})({name}))")
		checks.append(f"(if ({kind}.not-eq {name} {oracle_name}) (then (return {status}i32)))")
	(project / "oracle.h").write_text("\n".join(oracle) + "\n")
	source = (template.replace("LIMIT_SELECTORS", "\n\t\t".join(f"(value {name})" for name, _ in members))
		.replace("ORACLE_SELECTORS", "\n\t\t".join(oracles))
		.replace("ORACLE_DECLARATIONS", "\n\t\t".join(declarations))
		.replace("LIMIT_CHECKS", "\n\t\t\t".join(checks)))
	(project / "main.wove").write_text(source)


# Check the linked fixture's value comparisons without running its executable.
def assert_constant_result(clang, runtime, project, standard, flags, selected):
	source = project / ".wovec-cache/default/out/bin/main.c"
	result = subprocess.run([clang, f"-std={standard}", *flags, "-I", project, "-I", runtime,
		"-O2", "-S", "-emit-llvm", source, "-o", "-"],
		cwd=project, capture_output=True, text=True, timeout=30)
	assert result.returncode == 0, (project.name, result.stderr)
	entry = re.search(r"^define [^\n]* @wg[A-Za-z0-9_]*main\([^\n]*\{\n(.*?)^}", result.stdout, re.S | re.M)
	assert entry, (project.name, result.stdout)
	body = entry[1].strip().removeprefix("entry:").strip()
	returned = re.fullmatch(r"ret i32 ([0-9]+)", body)
	assert returned, (project.name, entry[1])
	status = int(returned[1])
	# A constant failure stores the manifest position in the return value.
	if 64 <= status < 64 + len(selected):
		raise AssertionError((project.name, selected[status - 64][0], "value mismatch"))
	assert status == 42, (project.name, status)


# Verify live bindings and one absent C23 selector when the frontend hides it.
def check_profile(compiler, clang, runtime, project, template, members, standard, flags):
	project.mkdir()
	native_names = macro_names(clang, standard, flags)
	required = {name for name, _, availability in members if availability == "required"}
	assert required <= native_names, (project.name, required - native_names)
	selected = [(name, kind) for name, kind, _ in members if name in native_names]
	config = {"profiles": [{"name": "default", "cc": {"cstd": standard},
		"build": {"default": {"cflags": [*flags, "-I.", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]}}}]}
	(project / "wovec.jsonc").write_text(json.dumps(config))
	stage_probe(project, template, selected)

	env = {**os.environ, "CLANG": clang}
	built = subprocess.run([compiler, "bin", "main.wove", "-o", "out"],
		cwd=project, env=env, capture_output=True, timeout=30)
	assert built.returncode == 0, (project.name, built.stderr)
	# Every typed comparison against the C oracle must fold to success.
	assert_constant_result(clang, runtime, project, standard, flags, selected)
	# An absent optional macro must report the original import name.
	if "BITINT_MAXWIDTH" not in native_names:
		source = '(module absent (use c-header "<limits.h>" (only (value BITINT_MAXWIDTH))))\n'
		(project / "absent.wove").write_text(source)
		absent = subprocess.run([compiler, "cc", "--check", "absent.wove"],
			cwd=project, env=env, capture_output=True, timeout=30)
		start = source.index("BITINT_MAXWIDTH")
		expected = {"code": 76, "message": "optional C integer macro is unavailable",
			"location": {"kind": "source", "file": "absent.wove", "start": start, "end": start + len("BITINT_MAXWIDTH")}}
		assert absent.returncode == 1, (project.name, absent)
		assert [json.loads(line) for line in absent.stderr.splitlines()] == [expected], (project.name, absent.stderr)
	return {name for name, _ in selected}


# Run from a relocated compiler and a clean project for each host profile.
def main():
	fixtures = Path(__file__).resolve().parent
	compiler, runtime = (Path(value).resolve() for value in sys.argv[1:])
	clang = os.environ.get("CLANG", "clang")
	template = (fixtures / "probe.wove").read_text()
	rows = (line.split("	") for line in (fixtures / "members.tsv").read_text().splitlines()
		if line and not line.startswith("#"))
	members = [(name, kind, availability) for name, kind, availability in rows
		if availability != "darwin" or sys.platform == "darwin"]
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
					template, members, standard, (*CLEAR_FEATURES, *flags)))
	print(f"limits.h: {len(covered)} live members checked across 12 host profiles")


if __name__ == "__main__":
	main()

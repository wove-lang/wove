# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory


# Read the platform's macro surface using the native CLI's feature definitions.
def fcntl_names(clang):
	prefix = b"#include <unistd.h>\n" if sys.platform == "linux" else b""
	capture = subprocess.check_output(
		[*clang, "-std=c11", "-D_GNU_SOURCE", "-D_DARWIN_C_SOURCE", "-dM", "-E", "-x", "c", "-"],
		input=prefix + b"#include <fcntl.h>\n",
	)
	return {line.split(None, 2)[1].decode("ascii", errors="replace") for line in capture.splitlines()
		if line.startswith(b"#define ")}


# Exercise the selected C spelling through an aliased value in a relocated compiler.
def check_member(compiler, stage, template, name, expected, available):
	project = stage / name
	project.mkdir()
	source = template.replace(b"TEST_CONSTANT", name.encode()).replace(b"TEST_VALUE", expected.encode())
	(project / "main.wove").write_bytes(source)
	# Available definitions must compile, link, and yield the header's audited value.
	if available:
		result = subprocess.run([compiler, "bin", "main.wove", "-o", "probe",
			"--cflag=-Wall", "--cflag=-Wextra", "--cflag=-Wpedantic", "--cflag=-Werror"],
			cwd=project, capture_output=True, timeout=30)
		assert result.returncode == 0, (name, result.stderr)
		probe = subprocess.run([project / "probe"], cwd=project, timeout=30)
		assert probe.returncode == 42, (name, probe.returncode)
		return
	# An older SDK can omit a member. Report at the requesting spelling.
	result = subprocess.run([compiler, "cc", "--check", "main.wove"],
		cwd=project, capture_output=True, timeout=30)
	assert result.returncode == 1, (name, result)
	start = source.index(name.encode())
	expected_error = {"code": 76, "message": "optional C integer macro is unavailable",
		"location": {"kind": "source", "file": "main.wove", "start": start, "end": start + len(name)}}
	assert [json.loads(line) for line in result.stderr.splitlines()] == [expected_error], (name, result.stderr)


# Check each optional member with the installed compiler and host SDK.
def main():
	fixtures = Path(__file__).resolve().parent
	compiler = Path(sys.argv[1]).resolve()
	template = (fixtures / "optional.wove").read_bytes()
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	names = fcntl_names(clang)
	# Keep the native working directory independent of any source checkout.
	with TemporaryDirectory(prefix="wove-fcntl-optional-") as directory:
		stage = Path(directory)
		installed = stage / compiler.name
		shutil.copy2(compiler, installed)
		# Only the active platform exposes each member's Wove declaration.
		for line in (fixtures / "optional.tsv").read_text().splitlines():
			# Ignore copyright and column labels.
			if not line or line.startswith("#"):
				continue
			platform, name, expected = line.split("\t")
			# Check the selected platform's declared members against the live SDK.
			if platform == sys.platform:
				check_member(installed, stage, template, name, expected, name in names)


if __name__ == "__main__":
	main()

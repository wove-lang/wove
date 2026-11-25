# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Run the native compiler and check its declared files, output, and diagnostics."""

import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


# Provide an executable wrapper and preserve a previous binary's bytes for failed builds.
def prepare_binary(root, clang, options, target):
	probe = root / options["probe"]
	probe.chmod(0o700)
	binary = root / target
	previous = binary.read_bytes() if binary.is_file() else None
	# An existing fixture executable must remain runnable after a failed tool invocation.
	if previous is not None:
		binary.chmod(0o700)
	env = {**os.environ, "CLANG": str(probe), "WOVE_CLANG_LOG": str(root / "clang.jsonl"),
		"WOVE_REAL_CLANG": json.dumps(clang)}
	env.pop("WOVE_CLANG_STATUS", None)
	# The fixture controls whether the probe delegates or writes a failed partial stage.
	if "tool_status" in options:
		env["WOVE_CLANG_STATUS"] = str(options["tool_status"])
	return env, previous


# Check the retained unit, single tool launch, executable publication, and diagnostic.
def check_binary(root, options, target, retained, value, diagnostic, result, patterns, previous):
	artifacts = sorted(path.relative_to(root).as_posix() for path in root.rglob("*.c") if path.is_file())
	assert artifacts == [retained], artifacts
	generated = (root / retained).read_text()
	# Apply the same retained-C pattern checks used by cc fixtures.
	for pattern, count in patterns.items():
		assert len(re.findall(pattern, generated)) == count, pattern
	calls = [json.loads(line) for line in (root / "clang.jsonl").read_text().splitlines()]
	expected = options.get(sys.platform, options["arguments"])
	assert len(calls) == 1 and len(calls[0]) == len(expected), calls
	# Fixture expressions account for the private paths chosen by the driver.
	for actual, pattern in zip(calls[0], expected, strict=True):
		assert re.fullmatch(pattern, actual), (actual, pattern)
	# Toolchain flags can request side outputs, which belong with retained C.
	for stage in root.rglob(".wovec-bin.*"):
		assert stage.parent == (root / retained).parent and not (stage / "program").exists(), stage
	binary = root / target
	# A failed build must preserve both the previously published bytes and its result.
	if diagnostic != "-":
		assert binary.read_bytes() == previous
		assert json.loads(result.stderr) == json.loads(diagnostic), result.stderr
	else:
		assert not result.stderr, result.stderr
	built = subprocess.run([binary], cwd=root, stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
	assert (built.returncode, built.stdout, built.stderr) == (int(value), b"", b""), built


# Exercise one invocation with the project and files specified by its fixture row.
def check_case(compiler, clang, runtime, fixtures, row):
	label, inputs, argv, status, target, retained, value, diagnostic, *options = row
	patterns = json.loads(options[0]) if options else {}
	flags = json.loads(options[1]) if len(options) > 1 else []
	binary = json.loads(options[2]) if len(options) > 2 else None
	with TemporaryDirectory(prefix="native-cli-", dir=compiler.parent) as workspace:
		root = Path(workspace)
		entries = json.loads(inputs)
		# Map fixture files to their supplied names in the command's working directory.
		for destination, source in entries.items():
			path = root / destination
			path.parent.mkdir(parents=True, exist_ok=True)
			# A hex fixture supplies exact raw bytes. Links are created after the regular files.
			if isinstance(source, dict):
				# Stage raw bytes now and validate deferred link descriptors.
				if "hex" in source:
					path.write_bytes(bytes.fromhex(source["hex"]))
				elif not {"hardlink", "symlink"} & source.keys():
					raise ValueError(f"unknown native fixture data for {destination}")
			else:
				shutil.copyfile(fixtures / source, path)
		# Link fixtures use workspace-relative targets, independent of checkout location.
		for destination, source in entries.items():
			# Preserve either shared inode identity or a distinct symbolic spelling.
			if isinstance(source, dict) and "hardlink" in source:
				os.link(root / source["hardlink"], root / destination)
			elif isinstance(source, dict) and "symlink" in source:
				(root / destination).symlink_to(source["symlink"])
		# Existing C paths can also be Wove inputs. A failed command must preserve their bytes.
		existing_c = {path.relative_to(root).as_posix(): path.read_bytes() for path in root.rglob("*.c") if path.is_file()}
		stages = {path.relative_to(root): path.read_bytes() for path in root.rglob("main.c.*") if path.is_file()}
		arguments = [argument if isinstance(argument, str) else bytes.fromhex(argument["hex"])
			for argument in json.loads(argv)]
		env, previous = prepare_binary(root, clang, binary, target) if binary else (None, None)
		result = subprocess.run([compiler, *arguments], cwd=root, env=env, capture_output=True, timeout=30)
		assert (result.returncode, result.stdout) == (int(status), b""), (label, result)
		# Binary fixtures run the executable published by the compiler itself.
		if binary:
			check_binary(root, binary, target, retained, value, diagnostic, result, patterns, previous)
			return
		# cc retains C beside its requested target without creating the target itself.
		assert not (root / target).exists(), (label, target)
		artifacts = sorted(path.relative_to(root).as_posix() for path in root.rglob("*.c") if path.is_file())
		assert {path.relative_to(root): path.read_bytes() for path in root.rglob("main.c.*") if path.is_file()} == stages, label
		if retained != "-":
			assert not result.stderr and artifacts == [retained], (label, result.stderr, artifacts)
			generated = (root / retained).read_text()
			# The case can count defining symbols without pinning installation-dependent hashes.
			for pattern, count in patterns.items():
				assert len(re.findall(pattern, generated)) == count, (label, pattern)
			program = root / "result"
			subprocess.run([*clang, *flags, "-I", "runtime", str(root / retained), runtime, "-o", str(program)],
				check=True, timeout=30)
			built = subprocess.run([program], cwd=root, stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
			assert (built.returncode, built.stdout, built.stderr) == (int(value), b"", b""), (label, built)
		else:
			assert artifacts == sorted(existing_c) and result.stderr.count(b"\n") == 1, (label, artifacts, result.stderr)
			assert all((root / path).read_bytes() == data for path, data in existing_c.items()), label
			assert json.loads(result.stderr) == json.loads(diagnostic), (label, result.stderr)


# Run each external scenario with the same compiler settings as the suite.
def main():
	compiler, manifest = (Path(value).resolve() for value in sys.argv[1:])
	clang = [*shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang")), "-std=c11",
		*shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))]
	runtime = os.environ.get("WOVE_TEST_RUNTIME", "runtime/wove_runtime.c")
	# Relocate the executable so its cases exercise an installed compiler outside the checkout.
	with TemporaryDirectory(prefix="native-wovec-") as install:
		installed = Path(install) / compiler.name
		shutil.copy2(compiler, installed)
		# The manifest supplies paths, argv, artifacts, and diagnostics.
		for line in manifest.read_text(encoding="utf-8").splitlines():
			# Ignore the manifest header and comments.
			if line and not line.startswith("#"):
				check_case(installed, clang, runtime, manifest.parent, line.split("\t"))


if __name__ == "__main__":
	main()

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


# Exercise one invocation with the project and files specified by its fixture row.
def check_case(compiler, clang, runtime, fixtures, row):
	label, inputs, argv, status, target, retained, value, diagnostic, *options = row
	patterns, = options or ["{}"]
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
		existing_c = {path.relative_to(root).as_posix(): path.read_bytes() for path in root.rglob("*.c")}
		arguments = [argument if isinstance(argument, str) else bytes.fromhex(argument["hex"])
			for argument in json.loads(argv)]
		result = subprocess.run([compiler, *arguments], cwd=root, capture_output=True, timeout=30)
		assert (result.returncode, result.stdout) == (int(status), b""), (label, result)
		# cc retains C beside its requested target without creating the target itself.
		assert not (root / target).exists(), (label, target)
		artifacts = sorted(path.relative_to(root).as_posix() for path in root.rglob("*.c"))
		if retained != "-":
			assert not result.stderr and artifacts == [retained], (label, result.stderr, artifacts)
			generated = (root / retained).read_text()
			# The case can count defining symbols without pinning installation-dependent hashes.
			for pattern, count in json.loads(patterns).items():
				assert len(re.findall(pattern, generated)) == count, (label, pattern)
			program = root / "result"
			subprocess.run([*clang, "-I", "runtime", str(root / retained), runtime, "-o", str(program)], check=True, timeout=30)
			built = subprocess.run([program], stdin=subprocess.DEVNULL, capture_output=True, timeout=30)
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

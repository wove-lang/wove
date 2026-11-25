# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Run the native compiler and check its declared files, output, and diagnostics."""

import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


# Exercise one invocation with the project and files specified by its fixture row.
def check_case(compiler, clang, runtime, fixtures, row):
	label, inputs, argv, status, target, retained, value, diagnostic = row
	with TemporaryDirectory(prefix="native-cli-", dir=compiler.parent) as workspace:
		root = Path(workspace)
		# Map fixture files to their supplied names in the command's working directory.
		for destination, source in json.loads(inputs).items():
			path = root / destination
			path.parent.mkdir(parents=True, exist_ok=True)
			# A hex value supplies exact raw input bytes without a checked-in binary.
			if isinstance(source, dict):
				path.write_bytes(bytes.fromhex(source["hex"]))
			else:
				shutil.copyfile(fixtures / source, path)
		arguments = [argument if isinstance(argument, str) else bytes.fromhex(argument["hex"])
			for argument in json.loads(argv)]
		result = subprocess.run([compiler, *arguments], cwd=root, capture_output=True, timeout=30)
		assert (result.returncode, result.stdout) == (int(status), b""), (label, result)
		# cc retains C beside its requested target without creating the target itself.
		assert not (root / target).exists(), (label, target)
		artifacts = sorted(path.relative_to(root).as_posix() for path in root.rglob("*.c"))
		if retained != "-":
			assert not result.stderr and artifacts == [retained], (label, result.stderr, artifacts)
			program = root / "result"
			subprocess.run([*clang, "-I", "runtime", str(root / retained), runtime, "-o", str(program)], check=True, timeout=30)
			built = subprocess.run([program], capture_output=True, timeout=30)
			assert (built.returncode, built.stdout, built.stderr) == (int(value), b"", b""), (label, built)
		else:
			assert not artifacts and result.stderr.count(b"\n") == 1, (label, artifacts, result.stderr)
			assert json.loads(result.stderr) == json.loads(diagnostic), (label, result.stderr)


# Run each external scenario with the same compiler settings as the suite.
def main():
	compiler, manifest = (Path(value).resolve() for value in sys.argv[1:])
	clang = [*shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang")), "-std=c11",
		*shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))]
	runtime = os.environ.get("WOVE_TEST_RUNTIME", "runtime/wove_runtime.c")
	# The manifest supplies paths, argv, publication, and the expected diagnostic.
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if line and not line.startswith("#"):
			check_case(compiler, clang, runtime, manifest.parent, line.split("\t"))


if __name__ == "__main__":
	main()

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Compare checked entry emission, bundled bindings, and validation order in both disk drivers."""

import json
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


# Copy manifest-supplied files into their paths in the isolated source graph.
def copy_inputs(root, fixtures, files):
	for target, source in files.items():
		path = root / target
		path.parent.mkdir(parents=True, exist_ok=True)
		shutil.copyfile(fixtures / source, path)


# Compare the first error's provider and operand span in both diagnostic formats.
def check_failure(bootstrap, native, root, expected):
	provider = root / expected["source"]
	operand = expected["operand"].encode()
	start = provider.read_bytes().index(operand)
	end = start + len(operand)
	error = expected["code"]
	private = subprocess.run(bootstrap, cwd=root, capture_output=True, timeout=30)
	assert private.returncode == 1 and not private.stdout, private
	assert private.stderr == f"{provider}:{start}-{end} wovec bootstrap error {error}\n".encode(), private
	ordinary = subprocess.run(native, cwd=root, capture_output=True, timeout=30)
	assert ordinary.returncode == 1 and not ordinary.stdout, ordinary
	assert json.loads(ordinary.stderr) == {"code": error, "message": expected["message"],
		"location": {"kind": "source", "file": str(provider), "start": start, "end": end}}, ordinary


# Check byte parity for one source graph before exercising any failed re-emissions.
def check_case(bootstrap, native, bundled, fixtures, label, case):
	with TemporaryDirectory(prefix="wove-emission-") as directory:
		root = Path(directory).resolve()
		shutil.copytree(bundled, root / "c-stdlib")
		copy_inputs(root, fixtures, case["files"])
		# Repeated byte cycles exercise asset size and content without storing a large binary.
		for target, cycle in case.get("byte_cycles", {}).items():
			path = root / target
			path.parent.mkdir(parents=True, exist_ok=True)
			path.write_bytes(bytes(range(cycle["width"])) * cycle["repeat"])

		entry = root / case["entry"]
		bootstrap_command = [bootstrap, entry]
		native_command = [native, "cc", entry, "-o", case["output"]]
		private = subprocess.run(bootstrap_command, cwd=root, capture_output=True, check=True, timeout=30)
		subprocess.run(native_command, cwd=root, check=True, timeout=30)
		retained = root / case["retained"]
		complete = retained.read_bytes()
		assert private.stdout == complete, label
		for expected in case.get("contains", []):
			assert expected.encode() in complete, (label, expected)

		# A failed re-emission leaves the successful unit intact.
		for failure in case.get("failures", []):
			for target in failure.get("remove", []):
				(root / target).unlink()
			copy_inputs(root, fixtures, failure.get("files", {}))
			check_failure(bootstrap_command, native_command, root, failure)
			assert complete == retained.read_bytes(), label


# Run each manifest case against the same two driver entry points.
def main():
	bootstrap, native, bundled, manifest = (Path(arg).resolve() for arg in sys.argv[1:])
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if line and not line.startswith("#"):
			label, case = line.split("\t", 1)
			check_case(bootstrap, native, bundled, manifest.parent, label, json.loads(case))


if __name__ == "__main__":
	main()

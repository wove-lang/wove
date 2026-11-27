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


# Compare the first error's provider and operand span in both compiler stages.
def check_failure(commands, root, expected):
	provider = root / expected["source"]
	operand = expected["operand"].encode()
	start = provider.read_bytes().index(operand)
	end = start + len(operand)
	error = expected["code"]
	for command in commands:
		result = subprocess.run(command, cwd=root, capture_output=True, timeout=30)
		assert result.returncode == 1 and not result.stdout, result
		assert json.loads(result.stderr) == {"code": error, "message": expected["message"],
			"location": {"kind": "source", "file": str(provider), "start": start, "end": end}}, result


# Check byte parity for one source graph before exercising any failed re-emissions.
def check_case(first, second, fixtures, label, case):
	with TemporaryDirectory(prefix="wove-emission-") as directory:
		root = Path(directory).resolve()
		copy_inputs(root, fixtures, case["files"])
		# Repeated byte cycles exercise asset size and content without storing a large binary.
		for target, cycle in case.get("byte_cycles", {}).items():
			path = root / target
			path.parent.mkdir(parents=True, exist_ok=True)
			path.write_bytes(bytes(range(cycle["width"])) * cycle["repeat"])

		entry = root / case["entry"]
		first_command = [first, "cc", entry, "-o", "first"]
		second_command = [second, "cc", entry, "-o", case["output"]]
		subprocess.run(first_command, cwd=root, check=True, timeout=30)
		subprocess.run(second_command, cwd=root, check=True, timeout=30)
		retained = root / case["retained"]
		complete = retained.read_bytes()
		first_retained = root / ".wovec-cache/default/first/bin/main.c"
		assert first_retained.read_bytes() == complete, label
		for expected in case.get("contains", []):
			assert expected.encode() in complete, (label, expected)

		# A fixture may forbid runtime work in the retained translation unit.
		for unexpected in case.get("excludes", []):
			assert unexpected.encode() not in complete, (label, unexpected)

		# A failed re-emission leaves the successful unit intact.
		for failure in case.get("failures", []):
			for target in failure.get("remove", []):
				(root / target).unlink()
			copy_inputs(root, fixtures, failure.get("files", {}))
			check_failure((first_command, second_command), root, failure)
			assert complete == first_retained.read_bytes(), label
			assert complete == retained.read_bytes(), label


# Run each manifest case against the same two driver entry points.
def main():
	first, second, manifest = (Path(arg).resolve() for arg in sys.argv[1:])
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if line and not line.startswith("#"):
			label, case = line.split("\t", 1)
			check_case(first, second, manifest.parent, label, json.loads(case))


if __name__ == "__main__":
	main()

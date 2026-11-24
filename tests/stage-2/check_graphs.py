# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import subprocess
import sys
from pathlib import Path


# Read one graph's file rows and expected probe output from the shared manifest.
def read_cases(manifest):
	arguments = []
	expected = []
	label = None
	# A case header starts a new argv and stdout expectation.
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if not line or line.startswith("#"):
			continue
		kind, *fields = line.split("\t")
		if kind == "case":
			if label is not None:
				yield label, arguments, expected
			label, entry, capacity = fields
			arguments = [entry, capacity]
			expected = []
		elif kind == "file" and label is not None:
			identity, virtual_path, source = fields
			arguments.extend((identity, virtual_path, source))
		elif kind == "expect" and label is not None:
			row, = fields
			expected.append(row)
		else:
			raise ValueError(f"invalid graph manifest record {kind}")
	if label is not None:
		yield label, arguments, expected


# Run each supplied graph through the same seed-built executable.
def main():
	probe, manifest = sys.argv[1:]
	# The fixture provides every path and expected row. The probe only formats results.
	for label, arguments, expected in read_cases(Path(manifest)):
		run = subprocess.run([probe, "--graph", *arguments], capture_output=True, text=True)
		status = int(expected[0].startswith("error "))
		actual = run.stdout.splitlines()
		if run.returncode != status or run.stderr or actual != expected:
			print(f"{label}: expected status {status}, got {run.returncode}", file=sys.stderr)
			print("expected:\n" + "\n".join(expected), file=sys.stderr)
			print(f"actual:\n{run.stdout}{run.stderr}", file=sys.stderr)
			return 1
	return 0


if __name__ == "__main__":
	sys.exit(main())

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Run the standalone project configuration assertions for each manifest row."""

from pathlib import Path
import subprocess
import sys


# Pass each source file, requested profile, and expected Wove result to the probe.
def main():
	probe, manifest = map(Path, sys.argv[1:])
	# Diagnostic offsets are half-open byte ranges in the original fixture.
	for row in manifest.read_text().splitlines():
		# Comments and blank lines group the case inventory.
		if not row or row.startswith("#"):
			continue
		source, profile, checks, error, start, end = row.split("\t")
		subprocess.run(
			[str(probe.resolve()), str(manifest.parent / source), "" if profile == "-" else profile,
			 checks, error, start, end],
			check=True,
		)


if __name__ == "__main__":
	main()

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Run the standalone project configuration assertions for each manifest row."""

from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory


# Pass each source file, requested profile, and expected Wove result to the probe.
def main():
	probe, manifest = map(Path, sys.argv[1:])
	# Diagnostic offsets are half-open byte ranges in the original fixture.
	with TemporaryDirectory(prefix="wove-config-") as work:
		for row in manifest.read_text().splitlines():
			# Comments and blank lines group the case inventory.
			if not row or row.startswith("#"):
				continue
			source, profile, checks, error, start, end = row.split("\t")
			path = manifest.parent / source
			# Enable examples. Baselines beside active overrides stay commented.
			if source.startswith("copy "):
				text = (manifest.parent / source.removeprefix("copy ")).read_text(encoding="utf-8")
				path = Path(work) / "copy.jsonc"
				path.write_text(re.sub(r'^([ \t]*)// ("[^"\n]+"[ \t]*:)', r"\1\2", text, flags=re.MULTILINE))
			subprocess.run(
				[str(probe.resolve()), str(path), "" if profile == "-" else profile,
				 checks, error, start, end],
				check=True,
			)


if __name__ == "__main__":
	main()

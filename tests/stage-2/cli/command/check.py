# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Run command resolution cases with the manifest's exact argv boundaries."""

import json
from pathlib import Path
import subprocess
import sys


# Deliver each argv array as separate host arguments to the standalone probe.
def main():
	probe, manifest = map(Path, sys.argv[1:])
	# Read compact JSON arrays so empty and space-containing operands survive.
	for row in manifest.read_text().splitlines():
		# Comments group the named cases.
		if not row or row.startswith("#"):
			continue
		name, config, checks, source, error, index, start, end, arguments = row.split("\t")
		path = config if config == "-" else str(manifest.parent / config)
		command = [str(probe.resolve()), path, checks, source, error, index, start, end, *json.loads(arguments)]
		# Report the case name together with the Wove fixture's assertion number.
		if subprocess.run(command).returncode:
			raise SystemExit(f"Command case failed: {name}")


if __name__ == "__main__":
	main()

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit this repository's compiler using the seed-built Wove emitter."""

import os
from pathlib import Path
import sys

from emit import emit_native
from inputs import collect_inputs
from target import compiler_target


ROOT = Path(__file__).resolve().parents[1]
ENTRY = "stage-2/wovec.wove"
VERSION_ASSET = "build/version.txt"


# Collect the fixed compiler graph, including its embedded bridge providers.
def compiler_inputs():
	sources, assets, _ = collect_inputs(ROOT, [ENTRY], embedded_providers=True)
	assets.pop(VERSION_ASSET, None)
	return sources, assets


# Forward the fixed compiler entry, assets and target to the shared emitter.
def main():
	build = ROOT / os.environ.get("WOVE_BUILD_DIR", "build")
	paths, assets = compiler_inputs()
	return emit_native(build / "stage-2/source_scan", build / "version.txt", paths, assets,
		root=ROOT, target=compiler_target())


# Print the generated unit for the ordinary build rule to publish.
if __name__ == "__main__":
	sys.exit(main())

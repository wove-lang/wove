# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit this repository's compiler using the seed-built Wove emitter."""

import os
from pathlib import Path
import platform
import sys

from emit import emit_native, native_target
from syntax import Form, parse


ROOT = Path(__file__).resolve().parents[1]
ENTRY = "stage-2/wovec.wove"
VERSION_ASSET = "build/version.txt"


# Follow the compiler's explicit Wove imports and collect its embedded assets.
def compiler_inputs():
	sources = {}
	assets = {}

	# Preserve first encounter order across the compiler's relative imports.
	def visit(path):
		if path in sources:
			return
		sources[path] = None
		parent = Path(path).parent

		# Only Wove imports and embed forms contribute further source paths.
		def walk(node):
			if not isinstance(node, Form) or node.tag == "disabled":
				return
			if node.tag == "use" and node.operands[0].text == "wove-file":
				visit(os.path.normpath(parent / node.operands[1].text))
			elif node.tag == "embed":
				assets[os.path.normpath(parent / node.operands[0].text)] = None
			else:
				for child in node.operands:
					walk(child)

		walk(parse((ROOT / path).read_text(encoding="utf-8")))

	visit(ENTRY)
	# The compiler's own embeds supply its bundled Wove bridge declarations.
	for path in tuple(assets):
		if Path(path).suffix == ".wove":
			visit(path)
	assets.pop(VERSION_ASSET, None)
	return sources, assets


# Forward the fixed compiler entry and the build host target to Wove.
def main():
	build = ROOT / os.environ.get("WOVE_BUILD_DIR", "build")
	paths, assets = compiler_inputs()
	os_name, arch, deployment = native_target()
	# Preserve the configured macOS deployment for the native driver source.
	if os_name == "macos":
		version = os.environ.get("MACOSX_DEPLOYMENT_TARGET") or platform.mac_ver()[0]
		if version:
			parts = map(int, version.split("."))
			deployment = str(sum(part * weight for part, weight in zip(parts, (10000, 100, 1))))
	return emit_native(build / "stage-2/source_scan", None, build / "version.txt", paths, assets,
		root=ROOT, target=(os_name, arch, deployment))


# Print the generated unit for the ordinary build rule to publish.
if __name__ == "__main__":
	sys.exit(main())

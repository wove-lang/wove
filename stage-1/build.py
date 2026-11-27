# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit this repository's compiler using the seed-built Wove emitter."""

import os
from pathlib import Path
import subprocess
import sys

from emit import emit_native
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


# Read the same target and deployment that Clang will use on the emitted C.
def compiler_target():
	environment = dict(os.environ)
	environment.setdefault("WOVE_BUILD_CLANG", "clang -std=c11")
	defines = subprocess.check_output(
		[ROOT / "tools/run-build-clang.sh", "-Qunused-arguments", "-dM", "-E", "-x", "c", "-"],
		input=b"", env=environment,
	)
	macros = {}
	# Clang also reports macros with an empty replacement list.
	for line in defines.decode().splitlines():
		name, _, value = line.removeprefix("#define ").partition(" ")
		macros[name] = value
	os_name = "macos" if "__APPLE__" in macros else "linux"
	arch = "aarch64" if "__aarch64__" in macros else "x86_64"
	deployment = macros.get("__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__", "-")
	# Older macOS deployments encode the minor and patch components as one digit each.
	if deployment != "-" and (version := int(deployment)) < 100000:
		deployment = str((version // 100) * 10000 + (version // 10 % 10) * 100 + version % 10)
	return os_name, arch, deployment


# Forward the fixed compiler entry, assets and target to the shared emitter.
def main():
	build = ROOT / os.environ.get("WOVE_BUILD_DIR", "build")
	paths, assets = compiler_inputs()
	return emit_native(build / "stage-2/source_scan", None, build / "version.txt", paths, assets,
		root=ROOT, target=compiler_target())


# Print the generated unit for the ordinary build rule to publish.
if __name__ == "__main__":
	sys.exit(main())

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Read the Clang target used to build a seed translation unit."""

from functools import cache
import os
from pathlib import Path
import subprocess


ROOT = Path(__file__).resolve().parents[1]


# Read the same target and deployment that Clang will use on the emitted C.
def compiler_target() -> tuple[str, str, str]:
	return clang_target(os.environ.get("WOVE_BUILD_CLANG", "clang -std=c11"))


# Cache the predefined macros' interpretation for this Clang command.
@cache
def clang_target(command: str) -> tuple[str, str, str]:
	environment = dict(os.environ, WOVE_BUILD_CLANG=command)
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

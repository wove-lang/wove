# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory


# Substitute platform values or splice the configured compiler argv as whole words.
def command_words(words, variables, tools):
	return [part for word in words for part in (tools[word] if word in tools else [word.format_map(variables)])]


# Run a manifest phase and verify the declared process exit and complete streams.
def run_phase(root, phase, variables, tools):
	argv = command_words(phase["argv"], variables, tools)
	result = subprocess.run(argv, cwd=root, capture_output=True, timeout=45)
	assert result.returncode == phase.get("exit", 0), (argv, result)
	assert re.fullmatch(phase.get("stdout", ""), result.stdout.decode()), (argv, result.stdout)
	stderr = result.stderr.decode()
	# Tool failures can print target-specific locations before the final Wove diagnostic.
	if "diagnostic" in phase:
		*prefix, last = stderr.splitlines()
		assert json.loads(last) == phase["diagnostic"], (argv, stderr)
		stderr = "\n".join(prefix)
	assert re.fullmatch(phase.get("stderr", ""), stderr, flags=re.DOTALL), (argv, stderr)


# Keep the original inode and contents of a regular artifact or a blocking directory.
def publication_snapshot(path):
	info = path.stat()
	# A directory fixture occupies a publication destination and must remain intact.
	if path.is_dir():
		return info.st_ino, tuple(sorted(entry.name for entry in path.iterdir()))
	return info.st_ino, path.read_bytes()


# Isolate the project, including the previous published artifact set for failure cases.
def check_case(fixtures, common, case, variables, tools):
	with TemporaryDirectory(prefix="wove-library-") as workspace:
		root = Path(workspace)
		# Test data, replacement sources, and all output paths come from the manifest.
		for destination, source in (common | case.get("files", {})).items():
			output = root / destination.format_map(variables)
			output.parent.mkdir(parents=True, exist_ok=True)
			shutil.copyfile(fixtures / source, output)
		# A setup build can use exactly the same installed compiler as the failure under test.
		for phase in case.get("prepare", []):
			run_phase(root, phase, variables, tools)
		# Failure scenarios select different exports so even a retained-C rewrite is observable.
		for destination, source in case.get("replace", {}).items():
			shutil.copyfile(fixtures / source, root / destination.format_map(variables))
		# A directory in place of a file forces a post-link publication failure.
		for name in case.get("blocked_destinations", []):
			path = root / name.format_map(variables)
			path.unlink(missing_ok=True)
			path.mkdir(parents=True)
		paths = [root / name.format_map(variables) for name in case.get("preserve", [])]
		before = {path: publication_snapshot(path) for path in paths}
		run_phase(root, case["build"], variables, tools)
		assert all(publication_snapshot(path) == original for path, original in before.items()), case["name"]
		# A rejected command may not leave the requested destinations or private stages behind.
		for pattern in case.get("absent", []):
			assert not list(root.glob(pattern.format_map(variables))), (case["name"], pattern)
		# Each generated artifact can carry platform-independent structural expectations.
		for name, expected in case.get("inspect", {}).items():
			path = root / name.format_map(variables)
			text = path.read_text()
			assert all(re.search(pattern, text) for pattern in expected.get("matches", [])), (path, expected)
			assert not any(re.search(pattern, text) for pattern in expected.get("omits", [])), (path, expected)
		# Native consumer and linker-introspection commands run only after publication.
		for phase in case.get("verify", []) + case.get(sys.platform, []):
			run_phase(root, phase, variables, tools)


# Relocate the compiler before running the project cases on the selected host toolchain.
def main(compiler, manifest):
	settings = json.loads("\n".join(line for line in manifest.read_text().splitlines() if not line.startswith("//")))
	variables = settings["platforms"][sys.platform]
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	# Copy the compiler independently from every fixture and any retained C directories.
	with TemporaryDirectory(prefix="wove-library-install-") as install:
		installed = Path(install) / compiler.name
		shutil.copy2(compiler, installed)
		tools = {"{wovec}": [str(installed)], "{clang}": clang}
		# Run the same manifest for macOS and Linux with their native symbol and loader tools.
		for case in settings["cases"]:
			check_case(manifest.parent, settings["files"], case, variables, tools)


if __name__ == "__main__":
	main(*(Path(value).resolve() for value in sys.argv[1:]))

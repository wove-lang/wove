# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory


# Substitute whole argv words while leaving whitespace and argument boundaries intact.
def expand(words, values, groups):
	return [argument for word in words for argument in (expand(groups[word], values, groups)
		if word in groups else [word.format_map(values)])]


# Run and check a phase with exact exit status and streams.
def run(args, root, expected, environment):
	result = subprocess.run(args, cwd=root, env=environment, capture_output=True, timeout=30)
	actual = result.returncode, result.stdout.decode(), result.stderr.decode()
	assert actual == (expected.get("exit", 0), expected.get("stdout", ""), expected.get("stderr", "")), (args, actual)


# Compare complete tool argument lists by output path. Completion order is immaterial.
def check_calls(root, expected, values, groups):
	actual = {str(path.relative_to(root)): json.loads(path.read_text()) for path in root.rglob("*.argv")}
	wanted = {path: expand(arguments, values, groups) for path, arguments in expected.items()}
	assert actual == wanted, (actual, wanted)


# Run an isolated staged build and apply the manifest's file, invocation and consumer checks.
def check_case(case, manifest, defaults, groups, probe, clang, target):
	with TemporaryDirectory(prefix="wove-staged-") as workspace:
		root = Path(workspace).resolve()
		values = {"root": root, "probe": probe, "clang": clang}
		case = {**defaults, **case}
		# Stage paths and contents come directly from the fixture.
		for path, source in case["files"].items():
			output = root / path
			output.parent.mkdir(parents=True, exist_ok=True)
			shutil.copyfile(manifest.parent / source, output)
		tool = case.get("tool")
		program = clang
		environment = os.environ.copy()
		# A controlled tool receives its action data separately from the argv under test.
		if tool:
			program = root / tool
			program.chmod(0o700)
			environment["WOVE_STAGED_ACTIONS"] = json.dumps(case["actions"])
		# Preserve both inode identity and bytes of any previously published entries.
		preserve = {path: ((root / path).stat().st_ino, (root / path).read_bytes()) for path in case.get("preserve", [])}
		config = (manifest.parent / case["config"]).read_text() if "config" in case else ""
		export = str(root / case["exports"]) if "exports" in case else ""
		units = [str(root / unit) for unit in case["units"]]
		args = [probe, program, root / case["image"], root / case["target"], target, export,
			config, str(case["budget"]), str(len(units)), *units, *expand(case["arguments"], values, groups)]
		run(args, root, case["build"], environment)
		# A missing link is verified both by the phase result and by the absence of a tool invocation.
		if "calls" in case:
			check_calls(root, case["calls"], values, groups)
		for path, (inode, contents) in preserve.items():
			assert ((root / path).stat().st_ino, (root / path).read_bytes()) == (inode, contents), path
		for path, contents in case.get("outputs", {}).items():
			assert (root / path).read_text() == contents, path
		for path in case.get("absent", []):
			assert not (root / path).exists(), path
		for phase in case.get("verify", []):
			run(expand(phase["argv"], values, groups), root, phase, environment)
	print(case["name"])


# Expand the manifest for the current host, applying an optional selected case filter.
def main(probe, clang, manifest_path, *selected):
	manifest = Path(manifest_path)
	lines = [line for line in manifest.read_text().splitlines() if not line.startswith("//")]
	data = json.loads("\n".join(lines))
	host = {"Darwin": "macos", "Linux": "linux"}[platform.system()]
	# Controlled cases may target a platform independently of the host.
	for case in data["cases"]:
		if (not selected or case["name"] in selected) and case.get("host", host) == host:
			check_case(case, manifest, data["defaults"], data["arguments"], Path(probe).resolve(), Path(clang).resolve(),
				case.get("os", host))


if __name__ == "__main__":
	main(*sys.argv[1:])

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

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from check import entry_snapshot, run_program


# Expand exact data, resolving paths chosen by the harness and splicing argv groups.
def words(items, values, groups):
	return [part for item in items for part in (words(groups[item], values, groups)
		if item in groups else [item.format_map(values)])]


# Require the expected compile actions before a single ordered link.
def check_calls(log, expected, values, groups):
	calls = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
	calls = [argv for argv in calls if "-###" not in argv]
	written = {argv[argv.index("-o") + 1].rsplit("/", 1)[-1]: argv for argv in calls}
	assert len(calls) == len(written) == len(expected), (calls, expected)
	for output, pattern in expected.items():
		actual = written[output]
		pattern = words(pattern, values, groups)
		assert len(actual) == len(pattern) and all(re.fullmatch(p, a) for p, a in zip(pattern, actual)), (actual, pattern)
	assert all("-c" in argv for argv in calls[:-1]), calls


# Build with a copied compiler from an isolated project and check the staged/public files.
def check_phase(phase, compiler, clang, root, manifest, values, groups):
	log = root / "clang.jsonl"
	log.unlink(missing_ok=True)
	if "previous" in phase:
		published = root / phase["previous"]["target"]
		shutil.copyfile(manifest / phase["previous"]["source"], published)
		published.chmod(0o700)
	preserved = {path: entry_snapshot(root / path) for path in phase.get("preserve", [])}
	env = {**os.environ, "CLANG": str(root / "clang"), "WOVE_PARALLEL_LOG": str(log),
		"WOVE_PARALLEL_CLANG": json.dumps(clang), "WOVE_PARALLEL_ACTIONS": json.dumps(phase.get("actions", {}))}
	result = subprocess.run([compiler, *words(phase["argv"], values, groups)], cwd=root, env=env,
		capture_output=True, timeout=30)
	assert result.returncode == phase.get("exit", 0), result
	assert result.stdout == phase.get("stdout", "").encode(), result
	# Parallel tool stderr is relayed in unit order before the ordinary diagnostic record.
	stderr = result.stderr.decode()
	if "diagnostic" in phase:
		prefix, diagnostic = stderr.rstrip("\n").rsplit("\n", 1) if "\n" in stderr.rstrip("\n") else ("", stderr)
		record = json.loads(diagnostic)
		diagnostic = phase["diagnostic"]
		assert record["code"] == diagnostic["code"], record
		assert re.fullmatch(diagnostic["message"], record["message"]), record
		assert record["location"] == diagnostic["location"], record
		stderr = prefix + "\n" if prefix else ""
	assert stderr == phase.get("stderr", ""), result
	check_calls(log, phase["calls"], values, groups)
	assert all(entry_snapshot(root / path) == old for path, old in preserved.items()), preserved
	assert all((root / path).is_file() for path in phase.get("present", [])), phase
	assert all(not (root / path).exists() for path in phase.get("absent", [])), phase
	assert not list(root.rglob(".wovec-bin.*")), phase
	if "run" in phase:
		run_program(root, root / phase["run"]["path"], phase["argv"], manifest, phase["run"])
	for path in phase.get("same_c", []):
		left, right = path
		assert (root / left).read_bytes() == (root / right).read_bytes(), path
	for consumer in phase.get("consumers", []):
		args = [*clang, *words(consumer["compile"], values, groups)]
		subprocess.run(args, cwd=root, check=True, timeout=30, capture_output=True)
		run_program(root, root / consumer["path"], phase["argv"], manifest, consumer)


# Resolve platform-specific data while keeping scenario selection in the manifest.
def main(compiler, manifest):
	manifest = Path(manifest).resolve()
	data = json.loads("\n".join(line for line in manifest.read_text().splitlines() if not line.startswith("//")))
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang")) + shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))
	system = data["platforms"][sys.platform]
	with TemporaryDirectory(prefix="wove-parallel-bin-") as work:
		parent = Path(work).resolve()
		installed = parent / "wovec"
		shutil.copy2(compiler, installed)
		for case in data["cases"]:
			root = parent / case["name"]
			root.mkdir()
			for path, fixture in (data["files"] | case.get("files", {})).items():
				dest = root / path
				dest.parent.mkdir(parents=True, exist_ok=True)
				shutil.copyfile(manifest.parent / fixture, dest)
			(root / "clang").chmod(0o700)
			values = {"root": str(root), **system["values"]}
			groups = data["arguments"] | system["arguments"]
			for phase in case["phases"]:
				check_phase(phase, installed, clang, root, manifest.parent, values, groups)
			print(case["name"])


if __name__ == "__main__":
	main(*sys.argv[1:])

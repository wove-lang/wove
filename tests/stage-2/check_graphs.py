# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
import shlex
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


# Read one graph's file rows and expected probe output from the shared manifest.
def read_cases(manifest):
	arguments = []
	expected = []
	label = None
	native = None
	# A case header starts a new argv and stdout expectation.
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if not line or line.startswith("#"):
			continue
		kind, _, payload = line.partition("\t")
		fields = payload.split("\t")
		if kind == "case":
			if label is not None:
				yield label, arguments, expected, native
			label, entry, capacity, *options = fields
			command = options[0] if options else "--graph"
			arguments = [command, entry, capacity, *options[1:]]
			expected = []
			native = None
		elif kind == "mode" and label is not None:
			arguments[:0] = ["--mode", payload]
		elif kind == "file" and label is not None:
			identity, virtual_path, source = fields
			arguments.extend((identity, virtual_path, source))
		elif kind == "native" and label is not None:
			status, *flags = fields
			native = int(status), flags, b""
		# Runtime traps use the same native recipe with exact stderr.
		elif kind == "native-trap" and label is not None:
			diagnostic, *flags = fields
			native = 1, flags, json.loads(diagnostic).encode("utf-8")
		elif kind == "expect" and label is not None:
			expected.append(payload)
		else:
			raise ValueError(f"invalid graph manifest record {kind}")
	if label is not None:
		yield label, arguments, expected, native


# Compile and run an emitted translation unit with the same compiler settings as the rest of the suite.
def check_native(probe, label, emitted, expected, fixture_flags, diagnostic):
	header, _, source = emitted.partition("\n")
	count = int(header.split()[1])
	assert len(source.encode()) == count, (label, header)
	compiler = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	flags = shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))
	runtime = os.environ.get("WOVE_TEST_RUNTIME", "runtime/wove_runtime.c")
	# Keep generated C and executables scoped to this fixture run.
	with TemporaryDirectory(prefix="graph-", dir=Path(probe).parent) as path:
		directory = Path(path)
		generated = directory / "emitted.c"
		program = directory / "emitted"
		generated.write_text(source, encoding="utf-8")
		command = [*compiler, "-std=c11", *flags, "-I", "runtime", *fixture_flags, str(generated), runtime, "-o", str(program)]
		subprocess.run(command, check=True, timeout=30)
		result = subprocess.run([program], capture_output=True, timeout=30)
		assert result.returncode == expected and not result.stdout and result.stderr == diagnostic, (label, result)


# Run each supplied graph through the seed-built executable.
def check_manifest(probe, manifest):
	# The fixture owns the graph, consumer mode, expected output, and optional native result.
	for label, arguments, expected, native in read_cases(Path(manifest)):
		run = subprocess.run([probe, *arguments], capture_output=True, text=True, timeout=30)
		status = int(expected[0].startswith("error "))
		actual = run.stdout.splitlines()
		if run.returncode != status or run.stderr or actual != expected:
			print(f"{label}: expected status {status}, got {run.returncode}", file=sys.stderr)
			print("expected:\n" + "\n".join(expected), file=sys.stderr)
			print(f"actual:\n{run.stdout}{run.stderr}", file=sys.stderr)
			return 1
		if native is not None:
			check_native(probe, label, run.stdout, *native)
	return 0


# Check resolutions and scalar programs from the configured fixture manifests.
def main():
	probe, *manifests = sys.argv[1:]
	# Each manifest is independent of the runner and shares the graph probe.
	for manifest in manifests:
		status = check_manifest(probe, manifest)
		if status:
			return status
	return 0


if __name__ == "__main__":
	sys.exit(main())

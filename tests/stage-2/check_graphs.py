# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
import shlex
import subprocess
import sys
from contextlib import nullcontext
from pathlib import Path
from tempfile import TemporaryDirectory


# Read one graph's file rows and expected probe output from the shared manifest.
def read_cases(manifest):
	arguments = []
	expected = []
	label = None
	prefix = False
	native = None
	assets = []
	# A case header starts a new argv and stdout expectation.
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if not line or line.startswith("#"):
			continue
		kind, _, payload = line.partition("\t")
		fields = payload.split("\t")
		if kind == "case":
			if label is not None:
				yield label, arguments, expected, prefix, native, assets
			label, entry, capacity, *options = fields
			command = options[0] if options else "--graph"
			arguments = [command, entry, capacity, *options[1:]]
			expected = []
			prefix = False
			native = None
			assets = []
		elif kind == "mode" and label is not None:
			arguments[:0] = ["--mode", payload]
		elif kind == "target" and label is not None:
			os_name, arch, deployment = fields
			arguments[:0] = ["--target", os_name, arch, deployment]
		elif kind == "file" and label is not None:
			identity, virtual_path, source = fields
			arguments.extend((identity, virtual_path, source))
		# Asset payloads are fixture data kept separate from the probe’s source rows.
		elif kind == "asset" and label is not None:
			path, contents = fields
			assets.append((path, b"" if contents == "-" else bytes.fromhex(contents)))
		# Platform-specific C headers are compiled on the corresponding host.
		elif kind in {"native", "native-on"} and label is not None:
			host, status, *flags = ["", *fields] if kind == "native" else fields
			native = host, flags, [(int(status), [], None, b"", b"")], None
		# Runtime traps use the same native recipe with exact stderr.
		elif kind == "native-trap" and label is not None:
			diagnostic, *flags = fields
			native = "", flags, [(1, [], None, b"", json.loads(diagnostic).encode("utf-8"))], None
		# Native ABI rejections are checked by the host compiler before linking.
		elif kind == "native-reject" and label is not None:
			diagnostics, *flags = fields
			native = "", flags, [], json.loads(diagnostics)
		# Additional vectors may supply a display name independent of the executable.
		elif kind == "native-argv" and native is not None:
			status, argv, *display = fields
			argv0, = display or ["null"]
			native[2].append((int(status), json.loads(argv), json.loads(argv0), b"", b""))
		# A program that reports results can attach expected streams to any invocation.
		elif kind == "native-output" and native is not None:
			stdout, stderr = (json.loads(field).encode("utf-8") for field in fields)
			status, argv, argv0, _, _ = native[2][-1]
			native[2][-1] = (status, argv, argv0, stdout, stderr)
		# A native behavior case may assert just the probe’s leading rows.
		elif kind in {"expect", "expect-prefix"} and label is not None:
			prefix = prefix or kind == "expect-prefix"
			expected.append(payload)
		else:
			raise ValueError(f"invalid graph manifest record {kind}")
	if label is not None:
		yield label, arguments, expected, prefix, native, assets


# Compile and run an emitted translation unit with the same compiler settings as the rest of the suite.
def check_native(probe, label, emitted, fixture_flags, executions, rejection):
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
		command = [*compiler, "-std=c11", *flags, "-I", "runtime"]
		# Match stable diagnostic text without depending on compiler-specific formatting.
		if rejection is not None:
			result = subprocess.run([*command, *fixture_flags, "-fsyntax-only", str(generated)], capture_output=True, text=True, timeout=30)
			assert result.returncode > 0 and all(text in result.stderr for text in rejection), (label, result)
			return
		# Place fixture libraries after the objects that reference them.
		subprocess.run([*command, str(generated), runtime, *fixture_flags, "-o", str(program)], check=True, timeout=30)
		# Reuse the compiled fixture for every supplied argument vector.
		for expected, argv, argv0, stdout, stderr in executions:
			result = subprocess.run([program if argv0 is None else argv0, *argv],
				executable=program, capture_output=True, timeout=30)
			assert (result.returncode, result.stdout, result.stderr) == (expected, stdout, stderr), (label, result)


# Materialize supplied assets for emission and move them before native verification.
def run_graph(probe, arguments, assets):
	context = TemporaryDirectory(prefix="assets-", dir=Path(probe).parent) if assets else nullcontext()
	with context as directory:
		paths = []
		command = [probe, *arguments]
		# Binary assets are independent of source file rows and may contain any bytes.
		for index, (virtual, contents) in enumerate(assets):
			path = Path(directory) / str(index)
			path.write_bytes(contents)
			paths.append(path)
			command.extend(("--asset", virtual, str(path)))
		result = subprocess.run(command, capture_output=True, text=True, timeout=30)
		# Retained C must compile and execute with every original asset path absent.
		for path in paths:
			path.rename(path.with_suffix(".moved"))
		return result


# Run each supplied graph through the seed-built executable.
def check_manifest(probe, manifest):
	# The fixture owns the graph, consumer mode, expected output, and optional native result.
	for label, arguments, expected, prefix, native, assets in read_cases(Path(manifest)):
		run = run_graph(probe, arguments, assets)
		status = int(expected[0].startswith("error "))
		actual = run.stdout.splitlines()
		if prefix:
			actual = actual[:len(expected)]
		if run.returncode != status or run.stderr or actual != expected:
			print(f"{label}: expected status {status}, got {run.returncode}", file=sys.stderr)
			print("expected:\n" + "\n".join(expected), file=sys.stderr)
			print(f"actual:\n{run.stdout}{run.stderr}", file=sys.stderr)
			return 1
		if native is not None and native[0] in ("", sys.platform):
			check_native(probe, label, run.stdout, *native[1:])
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

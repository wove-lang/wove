# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
from tempfile import TemporaryDirectory


# Run the same supplied graph with or without the probe's private partition flag.
def emit(probe, case, requested=None, capacity=None):
	options = ["--emit-graph"]
	bounds = [str(case["entry"]), str(case["scratch"]), str(case["capacity"] if capacity is None else capacity)]
	# The two probe commands differ only in how the checked graph is rendered.
	if requested is not None:
		options[0] = "--emit-graph-units"
		bounds.append(str(requested))
	files = [str(value) for row in case["files"] for value in row]
	return subprocess.run([probe, *options, *bounds, *files], capture_output=True, timeout=30)


# Recover complete C parts without relying on line breaks in the generated bodies.
def read_parts(output):
	lines, data = output.split(b"\n\n", 1)
	header, *rows = lines.decode("ascii").splitlines()
	assert header.split()[0] == "ok" and int(header.split()[1]) == len(data), header
	parts = {}
	cursor = 0
	# Each packet names a part and covers one consecutive extent in the stream.
	for row in rows:
		kind, name, length = row.split()
		end = cursor + int(length)
		assert kind == "unit" and cursor < end <= len(data) and name not in parts, row
		parts[name] = data[cursor:end]
		cursor = end
	assert cursor == len(data), (cursor, len(data))
	return parts


# Inspect public object symbols using the same names on Mach-O and ELF.
def symbols(path):
	result = subprocess.run(["nm", "-g", str(path)], check=True, capture_output=True, text=True, timeout=30)
	symbols = {}
	# Mach-O decorates C symbols with one leading underscore.
	for line in result.stdout.splitlines():
		fields = line.split()
		if len(fields) >= 2:
			kind, name = fields[-2:]
			symbols[name.removeprefix("_")] = kind
	return symbols


# Independently compile each part, verify its symbols, then link and run all objects together.
def check_native(probe, case, layout, parts):
	compiler = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	flags = shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))
	runtime = os.environ.get("WOVE_TEST_RUNTIME", "runtime/wove_runtime.c")
	# The ownership fixture uses the existing runtime allocation ledger.
	if case.get("tracked"):
		runtime = os.environ["WOVE_TEST_TRACKED_RUNTIME"]
	with TemporaryDirectory(prefix="units-", dir=Path(probe).parent) as temporary:
		root = Path(temporary)
		# Materialize all emitted files before compiling the individual translation units.
		for name, data in parts.items():
			(root / name).write_bytes(data)
		objects = []
		# Every expected C symbol is defined in its owning object and may be undefined elsewhere.
		for unit, names in layout["symbols"].items():
			object_file = (root / unit).with_suffix(".o")
			subprocess.run([*compiler, "-std=c11", *flags, "-I", "runtime", *case.get("cflags", []), "-c",
				str(root / unit), "-o", str(object_file)], check=True, timeout=30)
			table = symbols(object_file)
			assert {name for name, kind in table.items() if kind == "T" and name.startswith("wgf")} == set(names), (unit, table, names)
			for dependency in layout.get("imports", {}).get(unit, []):
				assert table.get(dependency) == "U", (unit, dependency, table)
			objects.append(str(object_file))
		program = root / "linked"
		subprocess.run([*compiler, "-std=c11", *flags, *objects, runtime, *case.get("link", []),
			"-o", str(program)], check=True, timeout=30)
		result = subprocess.run([program], capture_output=True, timeout=30)
	assert (result.returncode, result.stdout.decode(), result.stderr.decode()) == tuple(case["execution"]), (case["name"], result)


# Verify determinism, one-unit compatibility, real linkage, and atomic output on rejection.
def check_case(probe, case):
	baseline = None
	# Rejected graphs exercise the private gate and canary checks without compiling units.
	if "layouts" in case:
		baseline = emit(probe, case)
		assert baseline.returncode == 0 and not baseline.stderr, (case["name"], baseline)
	for layout in case.get("layouts", []):
		first = emit(probe, case, layout["requested"], layout.get("capacity"))
		second = emit(probe, case, layout["requested"], layout.get("capacity"))
		assert (first.returncode, first.stderr) == (second.returncode, second.stderr) == (0, b""), (case["name"], first, second)
		assert first.stdout == second.stdout, (case["name"], layout["requested"])
		parts = read_parts(first.stdout)
		assert list(parts) == layout["files"], (case["name"], list(parts), layout["files"])
		# One effective unit must retain every byte of the default serial emission.
		if len(parts) == 1:
			assert next(iter(parts.values())) == baseline.stdout.split(b"\n", 1)[1], case["name"]
		check_native(probe, case, layout, parts)
	for rejected in case.get("rejected", []):
		result = emit(probe, case, rejected["requested"], rejected.get("capacity"))
		assert result.returncode == 1 and not result.stderr and result.stdout.decode() == rejected["stdout"], (case["name"], result)


# Run the partition and native-link recipes supplied by the manifest.
def main():
	probe, manifest = sys.argv[1:]
	contents = Path(manifest).read_text(encoding="utf-8").splitlines()
	settings = json.loads("\n".join(line for line in contents if not line.startswith("//")))
	# Each graph supplies its own file rows, output partition and native expectations.
	for case in settings["cases"]:
		check_case(probe, case)


if __name__ == "__main__":
	main()

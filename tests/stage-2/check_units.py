# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
from tempfile import TemporaryDirectory


# Compile the audit and numbered files with identical header lookup and C flags.
def native_command(case, root):
	compiler = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	flags = shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))
	search = ["-I", str(root)] if "headers" in case else []
	return [*compiler, "-std=c11", *flags, "-I", "runtime", *search, *case.get("cflags", [])]


# Materialize the project headers beside the emitted C with their original spellings.
def stage_headers(case, root):
	for name, fixture in case.get("headers", []):
		path = root / name
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_bytes(Path(fixture).read_bytes())


# Certify the fixture's C prefix only when every output filename sees declaration-only C.
def audit_headers(probe, case, prefix):
	with TemporaryDirectory(prefix="unit-headers-", dir=Path(probe).parent) as temporary:
		root = Path(temporary)
		stage_headers(case, root)
		command = native_command(case, root)
		empty = subprocess.run([*command, "-E", "-dM", "-x", "c", "-"], input=b"",
			capture_output=True, check=True, timeout=30).stdout.splitlines()
		parts = {name for layout in case["layouts"] for name in layout["files"]}
		assert not parts.intersection(name for name, _ in case["headers"]), case["name"]
		snapshots = set()
		for name in sorted(parts.difference({"program.h"}) | {"serial.c"}):
			path = root / name
			path.write_bytes(prefix)
			macros = subprocess.run([*command, "-E", "-dM", str(path)],
				capture_output=True, check=True, timeout=30).stdout.splitlines()
			# Include guards and numeric context flags cannot inject per-call code or state.
			if any(not re.fullmatch(rb"#define [A-Za-z_][A-Za-z_0-9]*(?: +[0-9]+)? *", line)
				for line in set(macros).difference(empty)):
				return False
			preprocessed = subprocess.run([*command, "-E", "-P", str(path)],
				capture_output=True, check=True, timeout=30).stdout
			# A remaining preprocessing directive can modify a unit or emit linker metadata.
			if any(line.lstrip().startswith(b"#") for line in preprocessed.splitlines()):
				return False
			snapshots.add((frozenset(macros), preprocessed))
		if len(snapshots) != 1:
			return False
		ast = subprocess.run([*command, "-fsyntax-only", "-Xclang", "-ast-dump=json", str(path)],
			capture_output=True, check=True, timeout=30).stdout
	# Reject header objects, definitions, internal-linkage declarations, and native adapters.
	for declaration in json.loads(ast)["inner"]:
		kind = declaration["kind"]
		if kind in {"TypedefDecl", "RecordDecl", "EnumDecl"}:
			continue
		if (kind != "FunctionDecl" or declaration.get("storageClass", "extern") != "extern"
			or declaration.get("inline") or any(child["kind"] != "ParmVarDecl" for child in declaration.get("inner", []))):
			return False
	return True


# Run the same supplied graph with or without the probe's private partition flag.
def emit(probe, case, requested=None, capacity=None, prefix=b""):
	options = ["--emit-graph"]
	bounds = [str(case["entry"]), str(case["scratch"]), str(case["capacity"] if capacity is None else capacity)]
	# The two probe commands differ only in how the checked graph is rendered.
	if requested is not None:
		options[0] = "--emit-graph-units"
		bounds.append(str(requested))
		if prefix:
			bounds.extend(("--verified-c-prefix", prefix.decode()))
	if "mode" in case:
		options[:0] = ["--mode", case["mode"]]
	files = [str(value) for row in case["files"] for value in row]
	assets = []
	# Only emission can see the supplied assets. The later native build uses the emitted bytes.
	with TemporaryDirectory(prefix="unit-assets-") as temporary:
		for index, (virtual, payload) in enumerate(case.get("assets", [])):
			path = Path(temporary) / str(index)
			path.write_bytes(bytes.fromhex(payload))
			assets.extend(("--asset", virtual, str(path)))
		return subprocess.run([probe, *options, *bounds, *files, *assets], capture_output=True, timeout=30)


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
		stage_headers(case, root)
		# Materialize all emitted files before compiling the individual translation units.
		for name, data in parts.items():
			(root / name).write_bytes(data)
		objects = []
		# Every expected C symbol is defined in its owning object and may be undefined elsewhere.
		for unit, names in layout["symbols"].items():
			object_file = (root / unit).with_suffix(".o")
			subprocess.run([*native_command(case, root), "-c",
				str(root / unit), "-o", str(object_file)], check=True, timeout=30)
			table = symbols(object_file)
			if "entry_unit" in layout:
				assert (table.get("main") == "T") == (unit == layout["entry_unit"]), (unit, table)
			assert {name for name, kind in table.items() if kind == "T" and name.startswith("wgf")} == set(names), (unit, table, names)
			# Shared descriptors and values are external. Their byte backing arrays remain private.
			data = {name for name, kind in table.items() if kind != "U" and name.startswith(("wgb", "wga"))}
			assert data == set(layout.get("data", {}).get(unit, [])), (unit, data)
			for dependency in layout.get("imports", {}).get(unit, []):
				assert table.get(dependency) == "U", (unit, dependency, table)
			objects.append(str(object_file))
		program = root / "linked"
		# Fixture sources supply symbols before the runtime archive. Link options follow it.
		subprocess.run([*compiler, "-std=c11", *flags, *objects, *case.get("sources", []), runtime,
			*case.get("link", []), "-o", str(program)], check=True, timeout=30)
		executions = case["executions"] if "executions" in case else [([], *case["execution"])]
		for args, status, stdout, stderr in executions:
			result = subprocess.run([program, *args], capture_output=True, timeout=30)
			assert (result.returncode, result.stdout.decode(), result.stderr.decode()) == (status, stdout, stderr), (
				case["name"], result)


# Verify determinism, one-unit compatibility, real linkage, and atomic output on rejection.
def check_case(probe, case):
	baseline = None
	prefix = b""
	# Rejected graphs exercise the private gate and canary checks without compiling units.
	if "layouts" in case:
		baseline = emit(probe, case)
		assert baseline.returncode == 0 and not baseline.stderr, (case["name"], baseline)
	if "headers" in case:
		declared = b"".join(f"#include {operand}\n".encode() for operand in case["includes"])
		assert baseline.stdout.split(b"\n", 1)[1].startswith(declared), case["name"]
		verified = audit_headers(probe, case, declared)
		assert verified == case.get("replicable_headers", True), case["name"]
		if verified:
			prefix = declared
	for layout in case.get("layouts", []):
		first = emit(probe, case, layout["requested"], layout.get("capacity"), prefix)
		second = emit(probe, case, layout["requested"], layout.get("capacity"), prefix)
		assert (first.returncode, first.stderr) == (second.returncode, second.stderr) == (0, b""), (case["name"], first, second)
		assert first.stdout == second.stdout, (case["name"], layout["requested"])
		parts = read_parts(first.stdout)
		assert list(parts) == layout["files"], (case["name"], list(parts), layout["files"])
		# One effective unit must retain every byte of the default serial emission.
		if len(parts) == 1:
			assert next(iter(parts.values())) == baseline.stdout.split(b"\n", 1)[1], case["name"]
		if prefix and len(parts) > 1:
			for name, body in parts.items():
				if name.endswith(".c"):
					assert body.startswith(prefix + b'#include "program.h"\n'), (case["name"], name)
		check_native(probe, case, layout, parts)
	for rejected in case.get("rejected", []):
		verified_prefix = prefix if rejected.get("verified") else b""
		if rejected.get("reverse_prefix"):
			verified_prefix = b"".join(reversed(prefix.splitlines(keepends=True)))
		result = emit(probe, case, rejected["requested"], rejected.get("capacity"), verified_prefix)
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

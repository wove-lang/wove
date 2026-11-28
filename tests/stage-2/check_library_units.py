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

from check_units import emit, read_parts, symbols


RUNTIME = Path(__file__).resolve().parents[2] / "runtime"


# Save the real linker inputs before the serial driver cleans its private staging directory.
def capture_allowlist(destination, command):
	split = command.index("--")
	compiler, args = command[:split], command[split + 1:]
	for flag in ("-exported_symbols_list", "--version-script"):
		if flag in args:
			index = args.index(flag)
			assert args[index + 1] == "-Xlinker", args
			shutil.copyfile(args[index + 2], destination)
	os.execvp(compiler[0], [*compiler, *args])


# Compile a separate external consumer against the public header and selected library image.
def run_consumer(command, image, public_header, sources, fixture_root, expected):
	root = image.parent
	objects = []
	for index, (file, flags) in enumerate(sources):
		obj = root / f"consumer-{index}.o"
		subprocess.run([*command, "-std=c11", "-I", str(public_header.parent), *flags,
			"-c", str(fixture_root / file), "-o", str(obj)], check=True, timeout=30)
		objects.append(str(obj))
	executable = root / "consumer"
	subprocess.run([*command, *objects, str(image),
		"-Xlinker", "-rpath", "-Xlinker", str(root), "-o", str(executable)], check=True, timeout=30)
	result = subprocess.run([executable], capture_output=True, timeout=30)
	assert [result.returncode, result.stdout.decode(), result.stderr.decode()] == expected, (image, result)


# Read dynamic definitions only. An exact set also checks for leaked Wove and runtime names.
def dynamic_symbols(image, darwin):
	options = ["-gU"] if darwin else ["-D", "--defined-only"]
	output = subprocess.check_output(["nm", *options, str(image)], text=True, timeout=30)
	names = {line.split()[-1] for line in output.splitlines() if line.strip()}
	return {name.removeprefix("_") for name in names} if darwin else names


# Link independently compiled PIC objects through the serial library's exact allowlist.
def build_numbered(parts, root, image_name, allowlist, command, darwin, layout):
	root.mkdir()
	objects = []
	tables = {}
	flags = [*command, "-std=c11", "-fPIC", "-fvisibility=hidden", "-I", str(RUNTIME), "-pthread"]
	for name, data in parts.items():
		source = root / name
		source.write_bytes(data)
		if source.suffix == ".c":
			obj = source.with_suffix(".o")
			subprocess.run([*flags, "-c", str(source), "-o", str(obj)], check=True, timeout=30)
			objects.append(str(obj))
			tables[name] = symbols(obj)
	exports = set(layout["exports"])
	for unit, table in tables.items():
		definitions = {name for name, kind in table.items() if kind == "T" and name in exports}
		assert definitions == (exports if unit == layout["export_unit"] else set()), (unit, definitions)
	# Resolve checked private definitions and their consumers in distinct objects.
	for provider, consumer, pattern in layout["imports"]:
		names = {name for name, kind in tables[provider].items() if kind != "U" and re.fullmatch(pattern, name)}
		assert len(names) == 1, (provider, pattern, tables[provider])
		name = next(iter(names))
		assert tables[consumer].get(name) == "U", (consumer, name, tables[consumer])
	runtime = root / "runtime.o"
	subprocess.run([*flags, "-c", str(RUNTIME / "wove_runtime.c"), "-o", str(runtime)], check=True, timeout=30)
	mode, exports, identity = (
		("-dynamiclib", "-exported_symbols_list", ("-install_name", f"@rpath/{image_name}")) if darwin
		else ("-shared", "--version-script", ("-soname", image_name)))
	image = root / image_name
	subprocess.run([*command, mode, *objects, str(runtime), "-pthread", "-lm",
		"-Xlinker", exports, "-Xlinker", str(allowlist),
		"-Xlinker", identity[0], "-Xlinker", identity[1], "-o", str(image)], check=True, timeout=30)
	return image


# Verify each trampoline has one spelling in both signatures and no declarations in other parts.
def check_trampolines(serial, parts, layout):
	role = layout["trampoline_role"].encode("ascii")
	private = rb"w+g" + role + rb"[0-9A-Za-z]+_"
	owner = layout["export_unit"]
	# Public exports retain their source suffix even when the private prefix or ordinal changes.
	for symbol in layout["exports"]:
		declaration = re.compile(rb"\b(" + private + re.escape(symbol.encode("ascii")) + rb")\(")
		serial_names = declaration.findall(serial)
		assert len(serial_names) == 2 and serial_names[0] == serial_names[1], (symbol, serial_names)
		# Private declarations only appear in the unit that owns the wrapper definitions.
		for name, body in parts.items():
			assert declaration.findall(body) == (serial_names if name == owner else []), (symbol, name)


# Check both output forms against the same serial public interface and complete symbol allowlist.
def check_library(probe, wovec, target, darwin, root, clang, command, fixture_root, case):
	files = []
	for index, (name, fixture) in enumerate(case["files"]):
		source = root / name
		source.parent.mkdir(parents=True, exist_ok=True)
		shutil.copyfile(fixture_root / fixture, source)
		files.append([index, source, source])
	graph = dict(case, mode="library", target=target, files=files)
	entry = case["files"][case["entry"]][0]
	config = {"profiles": [{"name": "default", "entry": {"lib": [entry]}}]}
	(root / "wovec.jsonc").write_text(json.dumps(config))
	allowlist = root / "serial.exports"
	wrapper = root / "capture-clang"
	capture = [sys.executable, str(Path(__file__).resolve()), "--capture-allowlist", str(allowlist), *clang, "--"]
	wrapper.write_text(f'#!/bin/sh\nexec {shlex.join(capture)} "$@"\n')
	wrapper.chmod(0o755)
	image_name = case["image"] + (".dylib" if darwin else ".so")
	subprocess.run([wovec, "bin", entry, "-o", image_name], cwd=root,
		env=dict(os.environ, CLANG=str(wrapper)), check=True, timeout=60)
	public_header = root / (case["image"] + ".h")
	serial_image = root / image_name
	layout = case["layout"]
	exports = set(layout["exports"])
	assert dynamic_symbols(serial_image, darwin) == exports
	run_consumer(command, serial_image, public_header, case["consumer"], fixture_root, case["execution"])

	serial = emit(probe, graph)
	one = emit(probe, graph, 1)
	assert (serial.returncode, serial.stderr) == (one.returncode, one.stderr) == (0, b"")
	assert read_parts(one.stdout) == {"unit-0000000.c": serial.stdout.split(b"\n", 1)[1]}
	requested = layout["requested"]
	first, second = (emit(probe, graph, requested) for _ in range(2))
	assert (first.returncode, first.stderr) == (second.returncode, second.stderr) == (0, b""), (first, second)
	assert first.stdout == second.stdout
	parts = read_parts(first.stdout)
	assert list(parts) == layout["files"], list(parts)
	check_trampolines(serial.stdout.split(b"\n", 1)[1], parts, layout)
	width = sum(map(len, parts.values()))
	exact = emit(probe, graph, requested, width)
	short = emit(probe, graph, requested, width - 1)
	assert (exact.returncode, exact.stdout, exact.stderr) == (0, first.stdout, b"")
	assert short.returncode == 1 and short.stdout.startswith(b"error 16 ") and not short.stderr, short
	linked = build_numbered(parts, root / "numbered", image_name, allowlist, command, darwin, layout)
	assert dynamic_symbols(linked, darwin) == exports
	run_consumer(command, linked, public_header, case["consumer"], fixture_root, case["execution"])


# A request for numbered output must keep library-mode diagnostics and leave no partial C.
def check_diagnostics(probe, target, fixture_root, cases):
	for case in cases:
		files = [[i, name, fixture_root / file] for i, (name, file) in enumerate(case["files"])]
		graph = dict(case, mode="library", target=target, files=files)
		serial, split = emit(probe, graph), emit(probe, graph, case["requested"])
		assert (serial.returncode, serial.stdout, serial.stderr) == (split.returncode, split.stdout, split.stderr)
		assert split.returncode == 1 and split.stdout.startswith(case["diagnostic"].encode()) and not split.stderr, split


# Use the native Mach-O or ELF consumer and symbol tools on each supported host.
def main(probe, wovec, manifest):
	settings = json.loads("\n".join(line for line in manifest.read_text().splitlines() if not line.startswith("//")))
	darwin = sys.platform == "darwin"
	target = ["macos" if darwin else "linux", "-", "-"]
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	command = [*clang, *shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))]
	for case in settings["cases"]:
		with TemporaryDirectory(prefix="wove-numbered-library-") as temporary:
			check_library(probe, wovec, target, darwin, Path(temporary).resolve(), clang, command, manifest.parent, case)
	check_diagnostics(probe, target, manifest.parent, settings["rejected"])


if __name__ == "__main__":
	if sys.argv[1] == "--capture-allowlist":
		capture_allowlist(sys.argv[2], sys.argv[3:])
	else:
		main(*(Path(value).resolve() for value in sys.argv[1:]))

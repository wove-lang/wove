# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
from tempfile import TemporaryDirectory

from check_units import emit, read_parts, symbols


FIXTURES = Path(__file__).resolve().parent
LIBRARY = FIXTURES / "cli/native/library"
NUMBERED = FIXTURES / "numbered_units"
RUNTIME = FIXTURES.parent.parent / "runtime"
EXPORTS = {"numbered_answer", "byte_histogram", "bias_words", "shared_count", "mark_pair"}


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
def run_consumer(command, image, public_header):
	root = image.parent
	views = root / "consumer_views.o"
	subprocess.run([*command, "-std=c11", "-I", str(public_header.parent), "-Dmain=check_views",
		"-c", str(LIBRARY / "histogram-consumer.c"), "-o", str(views)], check=True, timeout=30)
	executable = root / "consumer"
	subprocess.run([*command, "-std=c11", "-I", str(public_header.parent),
		str(NUMBERED / "library_consumer.c"), str(views), str(image),
		"-Xlinker", "-rpath", "-Xlinker", str(root), "-o", str(executable)], check=True, timeout=30)
	result = subprocess.run([executable], capture_output=True, timeout=30)
	assert (result.returncode, result.stdout, result.stderr) == (42, b"", b""), (image, result)


# Read dynamic definitions only. An exact set also checks for leaked Wove and runtime names.
def dynamic_symbols(image, darwin):
	options = ["-gU"] if darwin else ["-D", "--defined-only"]
	output = subprocess.check_output(["nm", *options, str(image)], text=True, timeout=30)
	names = {line.split()[-1] for line in output.splitlines() if line.strip()}
	return {name.removeprefix("_") for name in names} if darwin else names


# Link independently compiled PIC objects through the serial library's exact allowlist.
def build_numbered(parts, root, image_name, allowlist, command, darwin):
	root.mkdir()
	objects = []
	tables = []
	flags = [*command, "-std=c11", "-fPIC", "-fvisibility=hidden", "-I", str(RUNTIME), "-pthread"]
	for name, data in parts.items():
		source = root / name
		source.write_bytes(data)
		if source.suffix == ".c":
			obj = source.with_suffix(".o")
			subprocess.run([*flags, "-c", str(source), "-o", str(obj)], check=True, timeout=30)
			objects.append(str(obj))
			tables.append(symbols(obj))
	for index, table in enumerate(tables):
		definitions = {name for name, kind in table.items() if kind == "T" and name in EXPORTS}
		assert definitions == (EXPORTS if index == len(tables) - 1 else set()), (index, definitions)
	# The wrapper unit imports an exported Wove body defined in an earlier unit.
	bodies = {name for table in tables[:-1] for name, kind in table.items()
		if kind == "T" and name.startswith("wgf") and name.endswith("numbered_answer")}
	assert len(bodies) == 1 and all(tables[-1].get(name) == "U" for name in bodies), tables
	# The scalar export body in turn imports its private helper from a different unit.
	helpers = {name for table in tables for name, kind in table.items()
		if kind == "T" and name.startswith("wgf") and name.endswith("offset")}
	assert len(helpers) == 1, tables
	helper = next(iter(helpers))
	assert any(table.get(helper) == "U" for table in tables), tables
	# The private helper uses immutable data owned by unit zero.
	data = {name for name, kind in tables[0].items()
		if kind != "U" and name.startswith("wgb") and name.endswith("bias")}
	assert len(data) == 1, tables
	bias = next(iter(data))
	assert any(table.get(bias) == "U" for table in tables[1:]), tables
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


# Check both output forms against the same serial public interface and complete symbol allowlist.
def check_library(probe, wovec, target, darwin, root, clang, command):
	files = []
	for index, fixture in enumerate((NUMBERED / "library.wove", NUMBERED / "library_support.wove", LIBRARY / "histogram.wove")):
		source = root / fixture.name
		shutil.copyfile(fixture, source)
		files.append([index, source, source])
	case = dict(mode="library", target=target, entry=0, scratch=8192, capacity=65536, files=files)
	(root / "wovec.jsonc").write_text('{"profiles":[{"name":"default","entry":{"lib":["library.wove"]}}]}\n')
	allowlist = root / "serial.exports"
	wrapper = root / "capture-clang"
	capture = [sys.executable, str(Path(__file__).resolve()), "--capture-allowlist", str(allowlist), *clang, "--"]
	wrapper.write_text(f'#!/bin/sh\nexec {shlex.join(capture)} "$@"\n')
	wrapper.chmod(0o755)
	image_name = "libhistogram.dylib" if darwin else "libhistogram.so"
	subprocess.run([wovec, "bin", "library.wove", "-o", image_name], cwd=root,
		env=dict(os.environ, CLANG=str(wrapper)), check=True, timeout=60)
	public_header = root / "libhistogram.h"
	serial_image = root / image_name
	assert dynamic_symbols(serial_image, darwin) == EXPORTS
	run_consumer(command, serial_image, public_header)

	serial = emit(probe, case)
	one = emit(probe, case, 1)
	assert (serial.returncode, serial.stderr) == (one.returncode, one.stderr) == (0, b"")
	assert read_parts(one.stdout) == {"unit-0000000.c": serial.stdout.split(b"\n", 1)[1]}
	first, second = (emit(probe, case, 3) for _ in range(2))
	assert (first.returncode, first.stderr) == (second.returncode, second.stderr) == (0, b""), (first, second)
	assert first.stdout == second.stdout
	parts = read_parts(first.stdout)
	assert list(parts) == ["program.h", "unit-0000000.c", "unit-0000001.c", "unit-0000002.c"], list(parts)
	assert b"wvex_" not in parts["program.h"]
	width = sum(map(len, parts.values()))
	exact = emit(probe, case, 3, width)
	short = emit(probe, case, 3, width - 1)
	assert (exact.returncode, exact.stdout, exact.stderr) == (0, first.stdout, b"")
	assert short.returncode == 1 and short.stdout.startswith(b"error 16 ") and not short.stderr, short
	linked = build_numbered(parts, root / "numbered", image_name, allowlist, command, darwin)
	assert dynamic_symbols(linked, darwin) == EXPORTS
	run_consumer(command, linked, public_header)


# A request for numbered output must keep library-mode diagnostics and leave no partial C.
def check_diagnostics(probe, target):
	for expected, fixtures in ((23, ["unsupported.wove"]), (33, ["reserved.wove"]),
			(32, ["scalars.wove", "duplicate.wove"])):
		files = [[i, f"/library/{'counters.wove' if name == 'duplicate.wove' else name}", LIBRARY / name]
			for i, name in enumerate(fixtures)]
		case = dict(mode="library", target=target, entry=0, scratch=8192, capacity=65536, files=files)
		serial, split = emit(probe, case), emit(probe, case, 3)
		assert (serial.returncode, serial.stdout, serial.stderr) == (split.returncode, split.stdout, split.stderr)
		assert split.returncode == 1 and split.stdout.startswith(f"error {expected} ".encode()) and not split.stderr, split


# Use the native Mach-O or ELF consumer and symbol tools on each supported host.
def main(probe, wovec):
	darwin = sys.platform == "darwin"
	target = ["macos" if darwin else "linux", "-", "-"]
	clang = shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang"))
	command = [*clang, *shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))]
	with TemporaryDirectory(prefix="wove-numbered-library-") as temporary:
		check_library(probe, wovec, target, darwin, Path(temporary).resolve(), clang, command)
	check_diagnostics(probe, target)


if __name__ == "__main__":
	if sys.argv[1] == "--capture-allowlist":
		capture_allowlist(sys.argv[2], sys.argv[3:])
	else:
		main(*(Path(value).resolve() for value in sys.argv[1:]))

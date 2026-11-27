# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Pass build inputs to the shared Wove C emitter."""

import platform
import subprocess
import sys
from pathlib import Path


# Select the fixed POSIX record view and cfg branches for the compiler's build host.
def native_target():
	os_name = "macos" if sys.platform == "darwin" else sys.platform
	machine = platform.machine()
	arch = {"arm64": "aarch64", "amd64": "x86_64"}.get(machine, machine)
	return os_name, arch, "-"


# Supply the compiler's full build graph to the seed-built emitter.
def emit_native(probe, destination, version, paths, providers, private_helpers=False, root=Path("."), target=None):
	files = [(index, "/" + name, str(root / name)) for index, name in enumerate(dict.fromkeys(paths))]
	rows = [str(field) for entry in files for field in entry]
	command = "--emit-private-graph" if private_helpers else "--emit-graph"
	capacity = max(4194304, 4 * sum(Path(source).stat().st_size for _, _, source in files))
	invocation = [probe, "--mode", "native", "--target", *(target or native_target()), command,
		"0", "8192", str(capacity), *rows]
	# The version keeps its virtual path even when Make uses a separate build directory.
	if version is not None:
		invocation.extend(("--asset", "/build/version.txt", version))
	# Give bundled source assets the paths their Wove embeds resolve against.
	for provider in providers:
		invocation.extend(("--asset", "/" + provider, str(root / provider)))
	result = subprocess.run(invocation, capture_output=True)
	header, separator, source = result.stdout.partition(b"\n")
	# Keep a bootstrap failure at its repository-relative source path.
	if result.returncode or not separator or not header.startswith(b"ok "):
		fields = header.split()
		if len(fields) >= 3 and fields[0] == b"error":
			file = int(fields[2])
			print(files[file][2] if 0 <= file < len(files) else paths[0], header.decode(), file=sys.stderr)
		else:
			sys.stderr.buffer.write(result.stderr or result.stdout)
		return 1
	# Make can publish the seed's stdout through its existing atomic build rule.
	if destination is None:
		sys.stdout.buffer.write(source)
	# The fixtures write directly to their generated input path.
	else:
		Path(destination).write_bytes(source)
	return 0


# Preserve the standalone emitter interface used by the test fixtures.
if __name__ == "__main__":
	probe, destination, version, *inputs = sys.argv[1:]
	private_helpers = inputs[:1] == ["--private-helpers"]
	# Separately linked runtime units request internal linkage for their generated helpers.
	if private_helpers:
		inputs = inputs[1:]
	# The optional asset delimiter keeps bundled files out of bootstrap resolution.
	if "--asset" in inputs:
		boundary = inputs.index("--asset")
		paths, providers = inputs[:boundary], inputs[boundary + 1:]
	else:
		paths, providers = inputs, []
	sys.exit(emit_native(probe, destination, version, paths, providers, private_helpers))

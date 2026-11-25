# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Supply the native compiler's build-time graph to the seed-built Stage 2 emitter."""

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
def emit_native(probe, destination, version, paths, providers, virtual_root="/"):
	# Separately linked drivers can have their own C symbol identities for shared modules.
	files = [(index, virtual_root + name, name) for index, name in enumerate(dict.fromkeys(paths))]
	rows = [str(field) for entry in files for field in entry]
	# The virtual path is relative to the Wove module even with a custom BUILD_DIR.
	invocation = [probe, "--mode", "native", "--target", *native_target(), "--emit-graph", "0", "8192", "4194304", *rows,
		"--asset", virtual_root + "build/version.txt", version]
	# Give bundled source assets the paths their Wove embeds resolve against.
	for provider in providers:
		invocation.extend(("--asset", virtual_root + provider, provider))
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
	if len(source) != int(header.split()[1]):
		print("native compiler: incomplete C translation unit", file=sys.stderr)
		return 1
	Path(destination).write_bytes(source)
	return 0


if __name__ == "__main__":
	probe, destination, version, *inputs = sys.argv[1:]
	virtual_root = "/"
	# A private virtual root gives a separately linked entry its own symbol identities.
	if inputs[:1] == ["--virtual-root"]:
		_, virtual_root, *inputs = inputs
	# The optional asset delimiter keeps bundled files out of bootstrap resolution.
	if "--asset" in inputs:
		boundary = inputs.index("--asset")
		paths, providers = inputs[:boundary], inputs[boundary + 1:]
	else:
		paths, providers = inputs, []
	sys.exit(emit_native(probe, destination, version, paths, providers, virtual_root))

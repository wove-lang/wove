# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Supply the native compiler's build-time graph to the seed-built Stage 2 emitter."""

import subprocess
import sys
from pathlib import Path


# The Makefile supplies all source paths before the native disk loader exists.
def emit_native(probe, destination, paths):
	files = [(index, "/" + name, name) for index, name in enumerate(dict.fromkeys(paths))]
	rows = [str(field) for entry in files for field in entry]
	invocation = [probe, "--mode", "native", "--emit-graph", "0", "8192", "4194304", *rows]
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
	probe, destination, *paths = sys.argv[1:]
	sys.exit(emit_native(probe, destination, paths))

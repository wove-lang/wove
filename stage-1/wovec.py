# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit C11 for a Stage 1 root and its local imports."""

from pathlib import Path
import sys

from compiler import compile_source
from syntax import SourceError


# Use stdout for generated C and report source failures by line.
def main() -> int:
	# The invocation names the executable root of the source graph.
	if len(sys.argv) != 2:
		print("usage: wovec.py SOURCE", file=sys.stderr)
		return 1
	filename = sys.argv[1]
	# Buffer the translation so a rejected source emits no C.
	try:
		output = compile_source(Path(filename).read_text(encoding="utf-8"), filename)
	# Show the line of a rejected source operation.
	except SourceError as error:
		print(f"{error.filename or filename}:{error.line}", file=sys.stderr)
		return 1
	# Forward file and encoding errors from the source reader.
	except (OSError, UnicodeError) as error:
		print(error, file=sys.stderr)
		return 1
	sys.stdout.write(output)
	return 0


# Leave the entry point idle when another module imports it.
if __name__ == "__main__":
	sys.exit(main())

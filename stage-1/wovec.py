# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit C11 for a Stage 1 root and its local imports."""

from pathlib import Path
import sys

from compiler import compile_source
from make_deps import write_make_deps
from syntax import SourceError


# Use stdout for generated C and report source failures by line.
def main() -> int:
	arguments = sys.argv[1:]
	imports = None
	# Make requests a dependency sidecar from the same successful compilation.
	if len(arguments) == 3 and arguments[0] == "--make-deps":
		_, destination, filename = arguments
		imports = {}
	# The invocation names the executable root of the source graph.
	elif len(arguments) == 1:
		filename, = arguments
	else:
		print("usage: wovec.py [--make-deps FILE] SOURCE", file=sys.stderr)
		return 1
	# Buffer the translation so a rejected source emits no C.
	try:
		output = compile_source(Path(filename).read_text(encoding="utf-8"), filename, imports=imports)
		# A failed graph never replaces the previous dependency inventory.
		if imports is not None:
			write_make_deps(destination, imports)
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

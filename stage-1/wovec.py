# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit C11 for one Stage 1 source file."""

from pathlib import Path
import sys

from compiler import compile_source
from syntax import SourceError


# Use stdout for generated C and report source failures by line.
def main() -> int:
	arguments = sys.argv[1:]
	bootstrap = arguments[:1] == ["--bootstrap"]
	# The optional flag affects selection throughout this compilation.
	if bootstrap:
		arguments = arguments[1:]
	# The seed translates one source file per invocation.
	if len(arguments) != 1:
		print("usage: wovec.py [--bootstrap] SOURCE", file=sys.stderr)
		return 1
	filename = arguments[0]
	# Buffer the translation so a rejected source emits no C.
	try:
		source = Path(filename).read_text(encoding="utf-8")
		output = compile_source(source, filename, bootstrap=bootstrap)
	# Show the line of a rejected source operation.
	except SourceError as error:
		print(f"{filename}:{error.line}", file=sys.stderr)
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

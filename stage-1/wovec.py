# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit C11 for a Stage 1 root and its local imports."""

import os
from pathlib import Path
import sys

from compiler import compile_source
from inputs import COMPILER_ENTRY, bundled_provider_inputs
from make_deps import write_make_deps
from syntax import SourceError


ROOT = Path(__file__).resolve().parents[1]


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
		path = Path(filename)
		bridge_files = ()
		# The fixed compiler build supplies the same bridge inventory as the native emitter.
		if path.resolve() == ROOT / COMPILER_ENTRY:
			inventory, providers = bundled_provider_inputs(ROOT)
			bridge_files = tuple(str(ROOT / provider) if path.is_absolute() else os.path.relpath(ROOT / provider)
				for provider in providers)
			# An edit to the inventory changes the declarations available to the seed.
			if imports is not None:
				for source in inventory:
					imports[str(ROOT / source) if path.is_absolute() else os.path.relpath(ROOT / source)] = None
		output = compile_source(path.read_text(encoding="utf-8"), filename, imports=imports, bridge_files=bridge_files)
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

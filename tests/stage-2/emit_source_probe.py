# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit the source probe's seed translation and aliases from one resolved graph."""

from argparse import ArgumentParser
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "stage-1"))
from compiler import Function, Module, Record, emit_graph, load_modules, read_module
from make_deps import write_make_deps
from syntax import SourceError


# Append aliases after the generated C so they are visible only to the surrounding harness.
def probe_aliases(root: Module) -> str:
	aliases = [
		f"#define {name} {function.c_name}"
		for name, function in root.bindings.items()
		if name not in root.functions and isinstance(function, Function)
	]
	aliases.extend(
		f"typedef {record.c_name} {name};"
		for name, record in root.type_bindings.items()
		if name not in root.records and isinstance(record, Record)
	)
	return "\n".join(aliases)


# Generate the alias block and dependency sidecar only when the full translation succeeds.
def main() -> int:
	parser = ArgumentParser(description=__doc__)
	parser.add_argument("--make-deps", required=True)
	parser.add_argument("source")
	args = parser.parse_args()
	imports = {}
	try:
		source = Path(args.source).read_text(encoding="utf-8")
		root = read_module(source, args.source, 0, 0, 0, entry=True, imports=imports)
		modules, headers = load_modules(root, imports=imports)
		output = emit_graph(modules, headers, imports=imports)
		aliases = probe_aliases(root)
		write_make_deps(args.make_deps, imports)
	except SourceError as error:
		print(f"{error.filename or args.source}:{error.line}", file=sys.stderr)
		return 1
	except (OSError, UnicodeError) as error:
		print(error, file=sys.stderr)
		return 1
	sys.stdout.write(output + "\n" + aliases + "\n")
	return 0


if __name__ == "__main__":
	sys.exit(main())

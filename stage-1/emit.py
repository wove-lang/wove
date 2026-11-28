# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Pass build inputs to the shared Wove C emitter."""

from argparse import ArgumentParser
import platform
import subprocess
import sys
from pathlib import Path

from inputs import collect_inputs
from make_deps import write_make_deps
from syntax import SourceError


BUNDLED_SOURCE = "stage-2/driver/bundled.wove"


# Select the fixed POSIX record view and cfg branches for the compiler's build host.
def native_target():
	os_name = "macos" if sys.platform == "darwin" else sys.platform
	machine = platform.machine()
	arch = {"arm64": "aarch64", "amd64": "x86_64"}.get(machine, machine)
	return os_name, arch, "-"


# Supply the ordered sources and assets to the seed-built native emitter.
def emit_native(probe, version, paths, assets, private_helpers=False, root=Path("."), target=None):
	files = [(index, "/" + name, str(root / name)) for index, name in enumerate(paths)]
	rows = [str(field) for entry in files for field in entry]
	command = "--emit-private-graph" if private_helpers else "--emit-graph"
	capacity = max(4194304, 4 * sum(Path(source).stat().st_size for _, _, source in files))
	invocation = [probe, "--mode", "native", "--target", *(target or native_target()), command,
		"0", "auto:8192", str(capacity), *rows]
	# The version keeps its virtual path even when Make uses a separate build directory.
	if version is not None:
		invocation.extend(("--asset", "/build/version.txt", version))
	# Give assets the virtual paths their Wove embeds resolve against.
	for asset in assets:
		invocation.extend(("--asset", "/" + asset, str(root / asset)))
	result = subprocess.run(invocation, capture_output=True)
	header, separator, source = result.stdout.partition(b"\n")
	# Keep a graph failure at its repository-relative source path.
	if result.returncode or not separator or not header.startswith(b"ok "):
		fields = header.split()
		# Graph-wide failures use the entry when there is no defining file.
		if len(fields) >= 3 and fields[0] == b"error":
			file = int(fields[2])
			print(files[file if 0 <= file < len(files) else 0][2], header.decode(), file=sys.stderr)
		else:
			sys.stderr.buffer.write(result.stderr or result.stdout)
		return 1
	sys.stdout.buffer.write(source)
	return 0


# Supply the requested fixture and the same bundled bridge inventory used by the compiler.
def fixture_inputs(root, entry):
	sources, assets, needs_bundle = collect_inputs(root, [entry], allow_missing_imports=True)
	bundle = {}
	# Pure Wove fixtures need no C bridge providers.
	if needs_bundle:
		bundle, inventory, _ = collect_inputs(root, [BUNDLED_SOURCE])
		providers = [asset for asset in inventory if Path(asset).suffix == ".wove"]
		bundled_sources, bundled_assets, _ = collect_inputs(root, providers, allow_missing_imports=True)
		sources.update(bundled_sources)
		assets.update(bundled_assets)
	# Leave missing assets out of the supplied table so active embeds get native diagnostics.
	assets = {asset: None for asset in assets if (root / asset).is_file()}
	return sources, assets, {**sources, **assets, **bundle}


# Emit one native fixture from its entry and record the successful input set for Make.
def main():
	parser = ArgumentParser(description=__doc__)
	parser.add_argument("probe")
	parser.add_argument("--private-helpers", action="store_true")
	parser.add_argument("--make-deps", required=True)
	parser.add_argument("entry")
	args = parser.parse_args()
	# Propagate the native emitter's source diagnostics without publishing failed dependencies.
	try:
		sources, assets, dependencies = fixture_inputs(Path("."), args.entry)
		status = emit_native(args.probe, None, sources, assets, args.private_helpers)
		# Make stages the C and dependencies until the complete invocation has succeeded.
		if status == 0:
			write_make_deps(args.make_deps, dependencies)
		return status
	except SourceError as error:
		print(f"{error.filename or args.entry}:{error.line}", file=sys.stderr)
	except (OSError, UnicodeError) as error:
		print(error, file=sys.stderr)
	return 1


# Keep the fixed bootstrap's import of the emitter free of command-line parsing.
if __name__ == "__main__":
	sys.exit(main())

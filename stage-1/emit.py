# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Pass standalone fixture inputs to the Wove source probe."""

from argparse import ArgumentParser
import platform
import subprocess
import sys
from pathlib import Path

from inputs import bundled_provider_inputs, collect_inputs
from make_deps import write_make_deps
from syntax import SourceError


# Select the fixed POSIX record view and cfg branches for the compiler's build host.
def native_target():
	os_name = "macos" if sys.platform == "darwin" else sys.platform
	machine = platform.machine()
	arch = {"arm64": "aarch64", "amd64": "x86_64"}.get(machine, machine)
	return os_name, arch, "-"


# Supply the ordered fixture sources and assets to the native emitter.
def emit_native(probe, paths, assets, private_helpers):
	files = [(index, "/" + name, name) for index, name in enumerate(paths)]
	rows = [str(field) for entry in files for field in entry]
	command = "--emit-private-graph" if private_helpers else "--emit-graph"
	capacity = max(4194304, 4 * sum(Path(source).stat().st_size for _, _, source in files))
	invocation = [probe, "--mode", "native", "--target", *native_target(), command,
		"0", "auto:8192", str(capacity), *rows]
	# Give assets the virtual paths their Wove embeds resolve against.
	for asset in assets:
		invocation.extend(("--asset", "/" + asset, asset))
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
		bundle, providers = bundled_provider_inputs(root)
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
		status = emit_native(args.probe, sources, assets, args.private_helpers)
		# Make stages the C and dependencies until the complete invocation has succeeded.
		if status == 0:
			write_make_deps(args.make_deps, dependencies)
		return status
	except SourceError as error:
		print(f"{error.filename or args.entry}:{error.line}", file=sys.stderr)
	except (OSError, UnicodeError) as error:
		print(error, file=sys.stderr)
	return 1


# Emit a standalone fixture when invoked by its Make recipe.
if __name__ == "__main__":
	sys.exit(main())

#!/usr/bin/env python3
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Record the native driver invocation and optionally fail after writing its stage."""

import json
import os
from pathlib import Path
import shutil
import sys


# Delegate to Clang after applying the fixture toolchain's spelling aliases.
def run_clang(arguments):
	clang = json.loads(os.environ["WOVE_REAL_CLANG"])
	aliases = json.loads(os.environ.get("WOVE_CLANG_TEST_ALIASES", "{}"))
	arguments = [aliases.get(argument, argument) for argument in arguments]
	os.execvp(clang[0], [*clang, *arguments])


# Record exactly what the launcher supplies before delegating to the test toolchain.
def main():
	arguments = sys.argv[1:]
	# An append records every launch, so retries or duplicate invocations fail the case.
	with Path(os.environ["WOVE_CLANG_LOG"]).open("a", encoding="utf-8") as log:
		log.write(json.dumps(arguments) + "\n")
	# Dry runs construct the real frontend job without producing an output artifact.
	if "-###" in arguments:
		run_clang(arguments)
	# A header query can synthesize a capture or fail before producing one.
	for flag, kind in (("-ast-dump=json", "AST"), ("-dM", "MACRO")):
		status = f"WOVE_CLANG_{kind}_STATUS"
		output = f"WOVE_CLANG_{kind}_OUTPUT"
		# Only replace the query that the fixture configured.
		if flag in arguments and (status in os.environ or output in os.environ):
			sys.stdout.write(os.environ.get(output, ""))
			return int(os.environ.get(status, "0"))
	# Capture the bytes presented to Clang before the driver can clean up its stage.
	if "WOVE_CLANG_CAPTURE_UNIT" in os.environ and "-o" in arguments:
		output = Path(arguments[arguments.index("-o") + 1])
		shutil.copyfile(output.with_name("main.c"), os.environ["WOVE_CLANG_CAPTURE_UNIT"])
	# A fixture can replace the tool with a staged-output and status probe.
	if "WOVE_CLANG_STATUS" in os.environ:
		output = Path(arguments[arguments.index("-o") + 1])
		# The output entry may be a relative symlink to a real executable in the stage.
		if "WOVE_CLANG_LINK_TARGET" in os.environ:
			executable = output.with_suffix(".real")
			shutil.copyfile(os.environ["WOVE_CLANG_LINK_TARGET"], executable)
			executable.chmod(0o700)
			output.symlink_to(executable.name)
		else:
			output.write_bytes(b"partial native output")
			# Only an explicit fixture option makes the simulated output usable.
			if os.environ.get("WOVE_CLANG_EXECUTABLE") == "1":
				output.chmod(0o700)
		# Only a fixture-supplied dependency is reported by a simulated frontend.
		if "WOVE_CLANG_DEPENDENCY" in os.environ:
			dependency = json.dumps(str(output.with_name(os.environ["WOVE_CLANG_DEPENDENCY"])))
			sys.stdout.write(f'digraph "dependencies" {{\n  header_0 [ shape="box", label={dependency}];\n}}\n')
		return int(os.environ["WOVE_CLANG_STATUS"])
	# A case can expect forwarded tool stdout while verifying the program's streams separately.
	if "WOVE_CLANG_STDOUT" in os.environ:
		sys.stdout.write(os.environ["WOVE_CLANG_STDOUT"])
		sys.stdout.flush()
	run_clang(arguments)


if __name__ == "__main__":
	sys.exit(main())

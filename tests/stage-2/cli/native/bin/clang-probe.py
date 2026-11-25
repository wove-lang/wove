#!/usr/bin/env python3
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Record the native driver invocation and optionally fail after writing its stage."""

import json
import os
from pathlib import Path
import shutil
import sys


# Record exactly what the launcher supplies before delegating to the test toolchain.
def main():
	arguments = sys.argv[1:]
	# An append records every launch, so retries or duplicate invocations fail the case.
	with Path(os.environ["WOVE_CLANG_LOG"]).open("a", encoding="utf-8") as log:
		log.write(json.dumps(arguments) + "\n")
	# An AST query can synthesize a capture or fail before producing one.
	if "-ast-dump=json" in arguments and ("WOVE_CLANG_AST_STATUS" in os.environ or "WOVE_CLANG_AST_OUTPUT" in os.environ):
		sys.stdout.write(os.environ.get("WOVE_CLANG_AST_OUTPUT", ""))
		return int(os.environ.get("WOVE_CLANG_AST_STATUS", "0"))
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
		# Only a fixture-supplied dependency is reported by a simulated frontend.
		if "WOVE_CLANG_DEPENDENCY" in os.environ:
			dependency = json.dumps(str(output.with_name(os.environ["WOVE_CLANG_DEPENDENCY"])))
			sys.stdout.write(f'digraph "dependencies" {{\n  header_0 [ shape="box", label={dependency}];\n}}\n')
		return int(os.environ["WOVE_CLANG_STATUS"])
	clang = json.loads(os.environ["WOVE_REAL_CLANG"])
	os.execvp(clang[0], [*clang, *arguments])


if __name__ == "__main__":
	sys.exit(main())

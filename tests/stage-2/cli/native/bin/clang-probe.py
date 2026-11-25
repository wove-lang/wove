#!/usr/bin/env python3
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Record the native driver invocation and optionally fail after writing its stage."""

import json
import os
from pathlib import Path
import sys


# Record exactly what the launcher supplies before delegating to the test toolchain.
def main():
	arguments = sys.argv[1:]
	# An append records every launch, so retries or duplicate invocations fail the case.
	with Path(os.environ["WOVE_CLANG_LOG"]).open("a", encoding="utf-8") as log:
		log.write(json.dumps(arguments) + "\n")
	# A fixture can replace the tool with a staged-output and status probe.
	if "WOVE_CLANG_STATUS" in os.environ:
		output = Path(arguments[arguments.index("-o") + 1])
		# The output entry may be a relative symlink to a real executable in the stage.
		if os.environ["WOVE_CLANG_OUTPUT"] == "symlink":
			executable = output.with_suffix(".real")
			executable.write_bytes(b"#!/bin/sh\nexit 42\n")
			executable.chmod(0o700)
			output.symlink_to(executable.name)
		else:
			output.write_bytes(b"partial native output")
		return int(os.environ["WOVE_CLANG_STATUS"])
	clang = json.loads(os.environ["WOVE_REAL_CLANG"])
	os.execvp(clang[0], [*clang, *arguments])


if __name__ == "__main__":
	sys.exit(main())

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Check the seed's complete and ordered include prefix."""

from pathlib import Path
import sys


# Match the checked-in graph against the C file already compiled by Make.
def check_includes(generated: Path) -> None:
	project = Path(__file__).resolve().parent / "imports/c_headers/project.h"
	expected = [
		'#include "wove_runtime.h"',
		"#include <stdint.h>",
		"#include <limits.h>",
		f'#include "{project}"',
		"#include <stddef.h>",
	]
	output = generated.read_text(encoding="utf-8")
	actual = [line for line in output.splitlines() if line.startswith("#include ")]
	assert actual == expected, (actual, expected)
	assert output.startswith("\n".join(expected) + "\n\n")


# Use the generated translation unit supplied by the fixture build.
if __name__ == "__main__":
	check_includes(Path(sys.argv[1]))

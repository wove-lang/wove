# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Check decoded seed string values and source lines."""

import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "stage-1"))
from syntax import Atom, Form, SourceError, parse


# Check the doc value and the source line of the declaration after it.
def check_value(source: str, expected: list[str]) -> None:
	text, literal_line, following_line = expected
	module = parse(source)
	doc, = (entry for entry in module.metadata if entry.tag == ":doc")
	value, = doc.operands
	assert value == Atom(json.loads(text), int(literal_line), quoted=True), value
	following = module.operands[-1]
	assert isinstance(following, Form) and following.line == int(following_line), following


# Require parse failures to identify the source line from the fixture.
def check_rejected(source: str, expected: list[str]) -> None:
	line, = expected
	try:
		parse(source)
	except SourceError as error:
		assert error.line == int(line), error.line
		return
	raise AssertionError("expected a source error")


# Read cases from the same manifest for both lexer spellings.
def main(manifest: Path) -> None:
	checks = {"value": check_value, "reject": check_rejected}
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if line and not line.startswith("#"):
			mode, label, source, *expected = line.split("\t")
			try:
				checks[mode](json.loads(source), expected)
			except AssertionError as error:
				raise AssertionError(label, *error.args) from error


if __name__ == "__main__":
	main(Path(sys.argv[1]))

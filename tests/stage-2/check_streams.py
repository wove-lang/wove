# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Compare whole streams from the seed-built Stage 2 probe."""

from itertools import zip_longest
from pathlib import Path
import re
import subprocess
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "stage-1"))
from syntax import TOKEN

KINDS = {"open": 40, "close": 41, "atom": 1, "string": 2, "comment": 59}


# Use the seed's universal-newline input while retaining each original byte boundary.
def seed_stream(view):
	text = []
	offsets = [0]
	for match in re.finditer(r"\r\n|.", view.decode("utf-8"), re.DOTALL):
		unit = match[0]
		text.append("\n" if unit in ("\r\n", "\r") else unit)
		offsets.append(offsets[-1] + len(unit.encode("utf-8")))

	for match in TOKEN.finditer("".join(text)):
		if match.lastgroup == "space":
			continue
		start = offsets[match.start()]
		end = offsets[match.end()]
		yield KINDS[match.lastgroup], start, end, match[0].encode("utf-8")
	yield 0, len(view), len(view), b""


# Use the decoded prefix to compute file coordinates independently of the scanner.
def position(source, offset):
	prefix = source[:offset].decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
	return prefix.count("\n") + 1, len(prefix.rsplit("\n", 1)[-1]) + 1


# Run a probe mode on the same checked file view and read its numeric rows.
def probe_rows(executable, mode, path, bounds):
	command = [str(executable), mode, str(path), *map(str, bounds)]
	result = subprocess.run(command, capture_output=True, check=False, timeout=30)
	assert not result.stderr, (path, result.stderr)
	rows = [tuple(map(int, row.split(b"\t"))) for row in result.stdout.splitlines()]
	return rows, result.returncode


# Check that the executable terminates the stream and advances every nonterminal row.
def read_stream(executable, path, view_length, bounds=()):
	rows, status = probe_rows(executable, "--tokens", path, bounds)
	assert rows, path
	cursor = 0
	for index, row in enumerate(rows):
		kind, start, end, error, _, _ = row
		assert cursor <= start <= end <= view_length, (path, row)
		if kind == 0 or error:
			assert index == len(rows) - 1, (path, row)
			if error:
				assert kind == 3 and end > start and status == 1, (path, row)
			else:
				assert start == end == view_length and status == 0, (path, row)
		else:
			assert kind != 3 and end > start, (path, row)
		cursor = end
	assert rows[-1][0] == 0 or rows[-1][3], path
	return rows


# Match raw spans, original spellings, and coordinates against the seed's lexer.
def compare_stream(executable, path, bounds=()):
	source = path.read_bytes()
	base, end = bounds or (0, len(source))
	view = source[base:end]
	rows = read_stream(executable, path, len(view), bounds)
	spellings = []
	for index, (actual, expected) in enumerate(zip_longest(rows, seed_stream(view))):
		assert actual is not None and expected is not None, (path, index, actual, expected)
		kind, start, stop, spelling = expected
		expected_row = (kind, start, stop, 0, *position(source, base + start))
		assert actual == expected_row, (path, index, actual, expected_row)
		assert view[start:stop] == spelling, (path, index, spelling)
		if kind != 0:
			spellings.append((kind, spelling))
	return spellings


# Read shared TSV fixture rows while preserving empty fields.
def fixture_rows(manifest):
	for line in manifest.read_text(encoding="utf-8").splitlines():
		if line and not line.startswith("#"):
			yield line.split("\t")


# Compare each configured rendering or embedded view to its original fixture.
def check_layouts(executable, directory, manifest):
	layout = directory / "layout.wove"
	for mode, label, source, *settings in fixture_rows(manifest):
		fixture = manifest.parent / source
		expected = compare_stream(executable, fixture)
		if mode == "render":
			prefix, separator, suffix = map(bytes.fromhex, settings)
			layout.write_bytes(prefix + separator.join(spelling for _, spelling in expected) + suffix)
			bounds = ()
		elif mode == "embed":
			prefix, suffix = map(bytes.fromhex, settings)
			view = fixture.read_bytes()
			layout.write_bytes(prefix + view + suffix)
			bounds = (len(prefix), len(prefix) + len(view))
		else:
			raise ValueError(f"{label}: unknown layout mode {mode!r}")
		assert compare_stream(executable, layout, bounds) == expected, label


# Exercise malformed bytes through the same executable and its checked file views.
def check_diagnostics(executable, directory, manifest):
	source = directory / "malformed.wove"
	for label, raw, base, end, *fields in fixture_rows(manifest):
		source.write_bytes(bytes.fromhex(raw))
		bounds = (int(base), int(end))
		expected = tuple(map(int, fields))
		rows = read_stream(executable, source, bounds[1] - bounds[0], bounds)
		assert rows[-1] == expected, (label, rows[-1], expected)
		if expected[3] == 5:
			assert len(rows) == 1, label


# Check result rows for the configured valid and malformed tagged forms.
def check_forms(executable, directory, manifest):
	source = directory / "form.wove"
	for label, raw, base, end, *fields in fixture_rows(manifest):
		source.write_bytes(bytes.fromhex(raw))
		expected = tuple(map(int, fields))
		rows, status = probe_rows(executable, "--form", source, (int(base), int(end)))
		assert rows == [expected], (label, rows, expected)
		assert status == int(expected[3] != 0), label


# Compare checked-in inputs and generated layout variants end to end.
def main():
	executable, layouts, diagnostics, forms, *sources = map(Path, sys.argv[1:])
	for source in sources:
		compare_stream(executable, source)

	with TemporaryDirectory(prefix="lexer-", dir=executable.parent) as path:
		directory = Path(path)
		check_layouts(executable, directory, layouts)
		check_diagnostics(executable, directory, diagnostics)
		check_forms(executable, directory, forms)


if __name__ == "__main__":
	main()

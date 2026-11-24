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
from compiler import metadata_enabled, selected_declarations
from syntax import TOKEN, parse

KINDS = {"open": 40, "close": 41, "atom": 1, "string": 2}


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
		yield KINDS[match.lastgroup], start, end, view[start:end]
	yield 0, len(view), len(view), b""


# Use the decoded prefix to compute file coordinates independently of the scanner.
def position(source, offset):
	prefix = source[:offset].decode("utf-8").replace("\r\n", "\n").replace("\r", "\n")
	return prefix.count("\n") + 1, len(prefix.rsplit("\n", 1)[-1]) + 1


# Run a probe mode on a file or a checked subrange and keep its output bytes.
def run_probe(executable, mode, path, bounds):
	command = [str(executable), mode, str(path), *map(str, bounds)]
	result = subprocess.run(command, capture_output=True, check=False, timeout=30)
	assert not result.stderr, (path, result.stderr)
	return result.stdout, result.returncode


# Read numeric rows from a probe mode.
def probe_rows(executable, mode, path, bounds):
	output, status = run_probe(executable, mode, path, bounds)
	rows = [tuple(map(int, row.split(b"\t"))) for row in output.splitlines()]
	return rows, status


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


# Compare every tree row, including all three links and the parent's coordinates.
def check_trees(executable, directory, manifest, mode):
	source = directory / "tree.wove"
	for label, raw, base, end, capacity, *fields in fixture_rows(manifest):
		source.write_bytes(bytes.fromhex(raw))
		expected = [tuple(map(int, row.split())) for row in fields]
		bounds = (int(base), int(end), int(capacity))
		rows, status = probe_rows(executable, mode, source, bounds)
		assert rows == expected, (label, rows, expected)
		assert status == int(expected[0][3] != 0), label


# Compare node kinds and links independently of byte offsets and layout.
def tree_shape(executable, source, bounds=()):
	rows, status = probe_rows(executable, "--metadata", source, bounds)
	assert status == 0 and rows[0][0] == len(rows) - 1, (source, rows)
	return [(index, kind, parent, child, sibling) for index, kind, _, _, parent, child, sibling, _, _ in rows[1:]]


# Check the renderer's byte count, exact layout, and successful parse-render-parse cases.
def check_renderings(executable, directory, manifest):
	source = directory / "render-input.wove"
	rendered = directory / "render-output.wove"
	# Both the original parent bytes and the expected output prefix come from the fixture.
	for label, raw, base, end, capacity, written, error, expected in fixture_rows(manifest):
		source.write_bytes(bytes.fromhex(raw))
		bounds = (int(base), int(end))
		expected_count, expected_error = int(written), int(error)
		output, status = run_probe(executable, "--render", source, (*bounds, int(capacity)))
		header, delimiter, prefix = output.partition(b"\n")
		assert delimiter, (label, output)
		assert tuple(map(int, header.split(b"\t"))) == (expected_count, expected_error), (label, header)
		expected_prefix = b"" if expected == "-" else bytes.fromhex(expected)
		assert len(prefix) == expected_count and prefix == expected_prefix, (label, prefix)
		assert status == int(expected_error != 0), label
		# Reparse the written bytes and compare both raw tokens and metadata-aware tree links.
		if status == 0:
			rendered.write_bytes(prefix)
			assert compare_stream(executable, source, bounds) == compare_stream(executable, rendered), label
			assert tree_shape(executable, source, bounds) == tree_shape(executable, rendered), label


# Compare exact decoded bytes and source spans, including prefixes on failure.
def check_decodings(executable, directory, manifest):
	source = directory / "decode-input.wove"
	for label, raw, base, end, capacity, result, expected in fixture_rows(manifest):
		parent = bytes.fromhex(raw)
		source.write_bytes(parent)
		start, stop = int(base), int(end)
		expected_row = tuple(map(int, result.split()))
		output, status = run_probe(executable, "--decode", source, (start, stop, int(capacity)))
		header, delimiter, prefix = output.partition(b"\n")
		assert delimiter and tuple(map(int, header.split(b"\t"))) == expected_row, (label, header)
		expected_prefix = b"" if expected == "-" else bytes.fromhex(expected)
		assert len(prefix) == expected_row[0] and prefix == expected_prefix, (label, prefix)
		assert status == int(expected_row[3] != 0), label
		# The seed is an independent reference for values representable in UTF-8.
		if status == 0:
			kind, _, _, spelling = next(seed_stream(parent[start:stop]))
			value, = parse("(value " + spelling.decode("utf-8") + ")").operands
			assert kind == 2 and value.quoted and prefix == value.text.encode("utf-8"), label


# Compare the sibling views with the seed's separated metadata and operand lists.
def compare_form_parts(view, nodes, parts, form):
	owner, head, metadata, operands = parts
	assert nodes[owner][5] == head
	_, _, start, end, *_ = nodes[head]
	assert view[start:end].decode("utf-8") == form.tag
	assert position(view, nodes[owner][2])[0] == form.line

	tags = []
	while metadata >= 0 and metadata != operands:
		_, _, start, end, *_ = nodes[nodes[metadata][5]]
		tags.append(view[start:end].decode("utf-8"))
		metadata = nodes[metadata][6]
	assert tags == [entry.tag for entry in form.metadata]

	count = 0
	while operands >= 0:
		count += 1
		operands = nodes[operands][6]
	assert count == len(form.operands)


# Verify exact result rows and compare successful module selection with the seed.
def check_modules(executable, directory, manifest):
	source = directory / "module.wove"
	for label, raw, base, end, capacity, *fields in fixture_rows(manifest):
		parent = bytes.fromhex(raw)
		source.write_bytes(parent)
		start, stop = int(base), int(end)
		expected = [tuple(map(int, row.split())) for row in fields]
		rows, status = probe_rows(executable, "--module", source, (start, stop, int(capacity)))
		assert rows == expected, (label, rows, expected)
		assert status == int(expected[0][3] != 0), label
		# Function bodies and import resolution are outside both selection walks.
		if status == 0:
			view = parent[start:stop]
			module = parse(view.decode("utf-8"))
			active = metadata_enabled(module, selectable=True)
			selected = list(selected_declarations(module.operands[1:])) if active else []
			name, emitted_active = rows[1]
			assert emitted_active == active and len(rows) == len(selected) + 3, label

			tree, tree_status = probe_rows(executable, "--metadata", source, (start, stop))
			assert tree_status == 0
			nodes = tree[1:]
			assert view[nodes[name][2]:nodes[name][3]].decode("utf-8") == module.operands[0].text
			for parts, form in zip(rows[2:], [module, *selected]):
				compare_form_parts(view, nodes, parts, form)


# Compare checked-in inputs and generated layout variants end to end.
def main():
	executable, layouts, diagnostics, forms, trees, metadata, renderings, decodings, modules, *sources = map(Path, sys.argv[1:])
	for source in sources:
		compare_stream(executable, source)

	with TemporaryDirectory(prefix="lexer-", dir=executable.parent) as path:
		directory = Path(path)
		check_layouts(executable, directory, layouts)
		check_diagnostics(executable, directory, diagnostics)
		check_forms(executable, directory, forms)
		check_trees(executable, directory, trees, "--tree")
		check_trees(executable, directory, metadata, "--metadata")
		check_renderings(executable, directory, renderings)
		check_decodings(executable, directory, decodings)
		check_modules(executable, directory, modules)


if __name__ == "__main__":
	main()

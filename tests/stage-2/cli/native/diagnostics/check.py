# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Check that native JSON and human diagnostics describe the same records."""

import json
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


# Return the diagnostic stream from a failed compilation or check.
def diagnostics(compiler, root, *args):
	result = subprocess.run([compiler, *args], cwd=root, capture_output=True, timeout=30)
	assert result.returncode == 1 and not result.stdout, result
	return result.stderr.decode("utf-8")


# Independently map a JSON byte offset to scalar coordinates in the original source.
def source_heading(root, record):
	location = record["location"]
	file = location["file"]
	source = (root / file).read_bytes()
	prefix = source[:location["start"]].decode("utf-8", errors="replace")
	lines = prefix.replace("\r\n", "\n").replace("\r", "\n").split("\n")
	return f"{file}:{len(lines)}:{len(lines[-1]) + 1}: error[{record['code']}]: {record['message']}"


# Compare both output styles for the same source failure and test the highlighted range.
def check_source(compiler, root, expected_line, expected_marker, *args):
	json_output = diagnostics(compiler, root, *args)
	record = json.loads(json_output)
	command, *operands = args
	human = diagnostics(compiler, root, command, "--human", *operands)
	assert human == f"{source_heading(root, record)}\n  {expected_line}\n  {expected_marker}\n", (record, human)
	return human, json_output


# Exercise original provider bytes, profile precedence, batched output, and failures before compilation.
def main():
	compiler = Path(sys.argv[1]).resolve()
	fixtures = Path(__file__).resolve().parent.parent
	with TemporaryDirectory(prefix="wove-human-") as directory:
		root = Path(directory)
		shutil.copyfile(fixtures / "imports/embed-root.wove", root / "main.wove")
		(root / "pkg").mkdir()
		provider = (fixtures / "emission/invalid.wove").read_text()
		indent = "\t\t\t"
		old = indent + '(let bytes (embed "./payload.bin"))\n' + indent + '(return (bool true))'
		new = '(let bytes (embed "./café.bin")) (return (bool true))'
		assert old in provider
		(root / "pkg/embedded.wove").write_bytes(provider.replace(old, indent + new).replace("\n", "\r\n").encode())
		line = indent + new + "))"
		marker = "\t\t\t" + " " * new.index("(bool true)") + "^" + "~" * (len("(bool true)") - 1)

		human, json_output = check_source(compiler, root, line, marker, "cc", "main.wove", "-o", "out")
		record = json.loads(json_output)
		location = record["location"]
		assert record["code"] == 24 and location["file"] == "pkg/embedded.wove"
		assert (root / location["file"]).read_bytes()[location["start"]:location["end"]] == b"(bool true)"
		assert diagnostics(compiler, root, "cc", "--check", "--human", "main.wove") == human
		assert diagnostics(compiler, root, "bin", "--human", "main.wove", "-o", "out") == human
		check_source(compiler, root, line, marker, "cc", "--check", str(root / "main.wove"))

		batch = diagnostics(compiler, root, "cc", "--check", "--human", "missing.wove", "main.wove")
		assert batch == f"missing.wove: error[64]: could not read file\n{human}", batch
		rows = [json.loads(line) for line in diagnostics(compiler, root, "cc", "--check", "missing.wove", "main.wove").splitlines()]
		assert rows[1] == record
		assert rows[0]["location"] == {"kind": "source", "file": "missing.wove", "start": 0, "end": 0}
		assert rows[0]["code"] == 64
		assert not (root / ".wovec-cache").exists() and not (root / "out").exists()

		project = root / "wovec.jsonc"
		project.write_text('{"profiles":[{"name":"default"},{"name":"readable","human":true}]}')
		assert diagnostics(compiler, root, "cc", "--check", "--profile", "readable", "main.wove") == human
		assert diagnostics(compiler, root, "cc", "--check", "--profile", "readable", "--no-human", "main.wove") == json_output

		assert diagnostics(compiler, root, "cc", "--human", "--unknown") == "argv[2]: error[1]: invalid command arguments\n"
		project.write_bytes(b'{"profiles":\n  }')
		check_source(compiler, root, "  }", "  ^", "cc", "--check", "main.wove")
		array_line = '{"profiles":[{"name":"default","human":['
		project.write_text(array_line + '\n  1\n]}]}')
		check_source(compiler, root, array_line, " " * (len(array_line) - 1) + "^ ...", "cc", "--check", "main.wove")
		project.unlink()
		project.mkdir()
		assert diagnostics(compiler, root, "cc", "--check", "--human", "main.wove") == "wovec.jsonc: error[64]: could not read file\n"
		project.rmdir()

		(root / "bad-utf8.wove").write_bytes(b"(module \xfe\x1b)")
		check_source(compiler, root, "(module ??)", "        ^", "cc", "--check", "bad-utf8.wove")
		(root / "empty.wove").touch()
		check_source(compiler, root, "", "^", "cc", "--check", "empty.wove")


if __name__ == "__main__":
	main()

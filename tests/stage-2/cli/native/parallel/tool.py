#!/usr/bin/env python3
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time


# Wait for a peer's marker, with a bounded failure for accidental serial scheduling.
def await_marker(path):
	deadline = time.monotonic() + 8
	while not path.exists():
		if time.monotonic() >= deadline:
			raise TimeoutError(path)
		time.sleep(0.005)


# Forward the actual frontend trace while applying the requested stdout control.
def run_probe(command, action):
	result = subprocess.run(command, stdout=subprocess.PIPE)
	if result.returncode:
		return result.returncode
	output = result.stdout
	assert len(output) >= action.get("min_stdout", 0), len(output)
	if "pad_stdout" in action:
		width = action["pad_stdout"]
		assert len(output) <= width, (len(output), width)
		output = output.ljust(width, b" ")
	if "shorten_stdout" in action:
		output = output[:action["shorten_stdout"]]
	if "replace_stdout" in action:
		pattern, replacement = action["replace_stdout"]
		output, count = re.subn(pattern.encode(), replacement.encode(), output)
		assert count > 0, pattern
	sys.stdout.buffer.write(output)
	sys.stdout.buffer.flush()
	if "truncate_stdout_file" in action:
		os.ftruncate(sys.stdout.fileno(), action["truncate_stdout_file"])
	return action.get("exit", 0)


# Record original argv and apply an optional fixture action around the real Clang call.
def main(arguments):
	fd = os.open(os.environ["WOVE_PARALLEL_LOG"], os.O_APPEND | os.O_CREAT | os.O_WRONLY, 0o600)
	try:
		os.write(fd, (json.dumps(arguments) + "\n").encode())
	finally:
		os.close(fd)
	# Restore normal toolchain lookup inside the fixture process, after Wove launched it.
	if "WOVE_FIXTURE_TOOL_PATH" in os.environ:
		os.environ["PATH"] = os.environ.pop("WOVE_FIXTURE_TOOL_PATH")
	clang = json.loads(os.environ["WOVE_PARALLEL_CLANG"])
	actions = json.loads(os.environ["WOVE_PARALLEL_ACTIONS"])
	if "-o" not in arguments and "-###" not in arguments:
		source = Path(arguments[-1])
		for action in actions.get("preflight", []):
			if action["flag"] not in arguments or ("file" in action and source.name != action["file"]):
				continue
			if "source_excludes" in action and action["source_excludes"].encode() in source.read_bytes():
				continue
			return run_probe([*clang, *arguments], action)
	if "-###" in arguments or "-o" not in arguments:
		os.execvp(clang[0], [*clang, *arguments])
	output = Path(arguments[arguments.index("-o") + 1])
	action = actions.get(output.name, {})
	for marker in action.get("start", []):
		Path(marker).touch()
	for marker in action.get("wait", []):
		await_marker(Path(marker))
	time.sleep(action.get("delay", 0))
	# Extra outputs model a compiler's private nested artifacts and symlinks.
	for filename, contents in action.get("files", {}).items():
		path = output.parent / filename
		path.parent.mkdir(parents=True, exist_ok=True)
		path.write_text(contents, encoding="utf-8")
	for filename, target in action.get("symlinks", {}).items():
		path = output.parent / filename
		path.parent.mkdir(parents=True, exist_ok=True)
		path.symlink_to(target)
	if "output" in action:
		output.write_text(action["output"], encoding="utf-8")
	sys.stdout.write(action.get("stdout", ""))
	sys.stdout.flush()
	sys.stderr.write(action.get("stderr", ""))
	sys.stderr.flush()
	status = action["exit"] if "exit" in action else subprocess.run([*clang, *arguments]).returncode
	for marker in action.get("finish", []):
		Path(marker).touch()
	return status if status >= 0 else 128 - status


if __name__ == "__main__":
	sys.exit(main(sys.argv[1:]))

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Write Make prerequisites from a successful seed build."""

from pathlib import Path


# Quote paths for a Make dependency file without expanding source text as Make syntax.
def make_word(path: str) -> str:
	# Make 3.81 must be able to use the same literal name as a target and a wildcard argument.
	if any(character in path for character in "\r\n:;|\\*?[]"):
		raise OSError(f"cannot write Make dependency for {path!r}")
	quoted = path.replace("$", "$$")
	# Escape literal whitespace and comment markers in file names.
	for character in " #\t":
		quoted = quoted.replace(character, "\\" + character)
	return quoted


# Keep missing imports visible despite the build's global .SECONDARY rule.
def write_make_deps(destination: str, imports: dict[str, None]) -> None:
	rules = []
	# Make supplies the target at include time, even when BUILD_DIR changes spelling.
	for filename in imports:
		quoted = make_word(filename)
		rules.extend((f"wove_seed_input := {quoted}", "$(wove_seed_target): $(wove_seed_input)",
			"ifeq ($(wildcard $(wove_seed_input)),)", ".PHONY: $(wove_seed_input)", "endif"))
	Path(destination).write_text("\n".join(rules) + "\n", encoding="utf-8")

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import os
import shutil
import sys
from pathlib import Path


# Top-level products and staging paths of a dedicated Make output directory.
BUILD_ENTRIES = {
	".wovec-cache", "editors", "self-host-smoke", "self-host-source",
	"stage-1", "stage-2", "version.txt", "wovec", "wovec.version",
}
BUILD_TEMP_PREFIXES = (
	".wovec-bin.", "self-host-check.", "self-host-source.tmp.", "version-check.", "version.txt.",
)


# Remove the build tree after checking the destination and custom output layout.
def clean_build(configured: str) -> None:
	# Require an explicit directory before resolving its path.
	if not configured.strip():
		raise ValueError("BUILD_DIR must name a dedicated output directory")
	path = Path(configured)
	root = Path(__file__).resolve().parent.parent
	# A directory link can select a tree outside the intended build location.
	if path.is_symlink():
		raise ValueError(f"refusing to clean a symbolic link: {path}")
	output = path.resolve()
	# Protect source checkouts and the directories containing them.
	if (output == root or output in root.parents
			or (output / ".git").exists() or (output / "Makefile").exists()):
		raise ValueError(f"refusing to clean {path}: expected a dedicated output directory")
	# An absent output directory is already clean.
	if not output.exists():
		return
	# Custom build directories must contain only the Make output layout.
	if output != root / "build":
		# Inspect all entries before removing any output.
		for entry in output.iterdir():
			if entry.name not in BUILD_ENTRIES and not entry.name.startswith(BUILD_TEMP_PREFIXES):
				raise ValueError(f"refusing to clean {path}: unexpected entry {entry.name!r}")
	shutil.rmtree(output)


# Report filesystem and configuration errors as Make failures.
if __name__ == "__main__":
	try:
		clean_build(os.environ["WOVE_CLEAN_BUILD_DIR"])
	except (OSError, ValueError) as error:
		sys.exit(f"clean: {error}")

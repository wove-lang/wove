# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Exercise both disk walkers after their source and graph tables grow."""

import json
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


CHAIN_LENGTH = 140
METADATA = '''
	(:copyright "Copyright (c) 2026 wove contributors")
	(:license "SPDX-License-Identifier: MIT OR Apache-2.0")
'''


# Write a module with the fixture declarations and standard source metadata.
def write_module(path, name, forms):
	path.write_text(f"(module {name}{METADATA}{forms}\n)\n", encoding="utf-8")


# Build a relative chain with a shared provider and enough files to grow both drivers' tables.
def write_graph(root, bundled):
	shutil.copytree(bundled, root / "llvm-libc")
	chain = root / "chain"
	chain.mkdir()
	write_module(root / "shared.wove", "shared",
		"\t(fn shared_value (params) (returns i32) (body (return 42i32)))")
	for index in range(CHAIN_LENGTH):
		name = f"hop_{index}"
		following = f"hop_{index + 1}"
		uses = '\t(use wove-file "../shared.wove" (only shared_value))'
		value = "(call shared_value)"
		# Keep the shared edge active at every depth and forward only through the chain.
		if index + 1 < CHAIN_LENGTH:
			uses += f'\n\t(use wove-file "./{following}.wove" (only {following}))'
			value = f"(call {following})"
		write_module(chain / f"{name}.wove", name,
			f"{uses}\n\t(fn {name} (params) (returns i32) (body (return {value})))")
	entry = root / "main.wove"
	write_module(entry, "discovery_entry", '''	(use wove-file "./chain/hop_0.wove" (only hop_0))
	(fn main (params (argv (shared-array (shared-array u8))))
		(returns i32) (body (return (call hop_0))))''')
	return entry


# Report the first failing edge at the final provider after both workspaces grow.
def check_edge_failure(root, entry, bootstrap, native, requested, code):
	name = f"hop_{CHAIN_LENGTH - 1}"
	terminal = root / "chain" / f"{name}.wove"
	failed_use = f'(use wove-file "{requested}" (only unreachable))'
	later_use = '(use wove-file "./later-missing.wove" (only absent))'
	write_module(terminal, name,
		f"\t{failed_use}\n\t{later_use}\n\t(fn {name} (params) (returns i32) (body (return 0i32)))")
	start = terminal.read_bytes().index(failed_use.encode())
	end = start + len(failed_use)
	private = subprocess.run([bootstrap, entry], cwd=root, capture_output=True, timeout=30)
	assert private.returncode == 1 and not private.stdout, private
	assert private.stderr == f"{terminal}:{start}-{end} wovec bootstrap error {code}\n".encode(), private
	ordinary = subprocess.run([native, "cc", entry, "-o", "failed"], cwd=root, capture_output=True, timeout=30)
	assert ordinary.returncode == 1 and not ordinary.stdout, ordinary
	diagnostic = json.loads(ordinary.stderr)
	assert diagnostic["code"] == code, diagnostic
	assert diagnostic["location"] == {"kind": "source", "file": str(terminal), "start": start, "end": end}, diagnostic
	assert not (root / ".wovec-cache/default/failed/bin/main.c").exists()


# Compare both emitted units and execute the same reached call chain.
def main():
	bootstrap, native, bundled = (Path(arg).resolve() for arg in sys.argv[1:])
	with TemporaryDirectory(prefix="wove-discovery-") as directory:
		root = Path(directory).resolve()
		entry = write_graph(root, bundled)
		entries = set(root.rglob("*"))
		checked = subprocess.run([native, "cc", "--check", entry], cwd=root, capture_output=True, check=True, timeout=30)
		assert not checked.stdout and not checked.stderr, checked
		assert set(root.rglob("*")) == entries
		seed = subprocess.run([bootstrap, entry], cwd=root, capture_output=True, check=True, timeout=30)
		subprocess.run([native, "bin", entry, "-o", "discovery"], cwd=root, check=True, timeout=30)
		assert seed.stdout == (root / ".wovec-cache/default/discovery/bin/main.c").read_bytes()
		run = subprocess.run([root / "discovery"], cwd=root, timeout=30)
		assert run.returncode == 42, run
		check_edge_failure(root, entry, bootstrap, native, "./missing.wove", 27)
		check_edge_failure(root, entry, bootstrap, native, "./hop_0.wove", 28)


if __name__ == "__main__":
	main()

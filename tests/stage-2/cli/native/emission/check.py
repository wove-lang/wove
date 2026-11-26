# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Compare checked entry emission, bundled bindings, and validation order in both disk drivers."""

import json
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


# Compare the first error's provider and operand span in both diagnostic formats.
def check_failure(bootstrap, native, root, entry, provider, error, message, operand):
	start = provider.read_bytes().index(operand)
	end = start + len(operand)
	private = subprocess.run([bootstrap, entry], cwd=root, capture_output=True, timeout=30)
	assert private.returncode == 1 and not private.stdout, private
	assert private.stderr == f"{provider}:{start}-{end} wovec bootstrap error {error}\n".encode(), private
	ordinary = subprocess.run([native, "cc", entry, "-o", "embeds"], cwd=root, capture_output=True, timeout=30)
	assert ordinary.returncode == 1 and not ordinary.stdout, ordinary
	assert json.loads(ordinary.stderr) == {"code": error, "message": message,
		"location": {"kind": "source", "file": str(provider), "start": start, "end": end}}, ordinary


# Exercise successful output, unavailable embeds, and an earlier semantic failure.
def main():
	bootstrap, native, bundled = (Path(arg).resolve() for arg in sys.argv[1:])
	cases = Path(__file__).resolve().parent
	imports = cases.parent / "imports"
	with TemporaryDirectory(prefix="wove-emission-") as directory:
		root = Path(directory).resolve()
		shutil.copytree(bundled, root / "c-stdlib")
		entry = root / "main.wove"
		shutil.copyfile(imports / "embed-root.wove", entry)
		pkg = root / "pkg"
		pkg.mkdir()
		provider = pkg / "embedded.wove"
		shutil.copyfile(imports / "embedded.wove", provider)
		payload = pkg / "payload.bin"
		payload.write_bytes(bytes(range(256)) * 128)

		private = subprocess.run([bootstrap, entry], cwd=root, capture_output=True, check=True, timeout=30)
		subprocess.run([native, "cc", entry, "-o", "embeds"], cwd=root, check=True, timeout=30)
		retained = root / ".wovec-cache/default/embeds/bin/main.c"
		complete = retained.read_bytes()
		assert private.stdout == complete

		payload.unlink()
		check_failure(bootstrap, native, root, entry, provider, 67, "could not read embedded asset", b'"./payload.bin"')
		assert complete == retained.read_bytes()
		shutil.copyfile(cases / "invalid.wove", provider)
		check_failure(bootstrap, native, root, entry, provider, 24, "invalid Wove source", b"(bool true)")
		assert complete == retained.read_bytes()

		# Both loaders give the new math provider the same identity and bindings.
		math = root / "math.wove"
		shutil.copyfile(cases.parent / "math" / "values.wove", math)
		private_math = subprocess.run([bootstrap, math], cwd=root, capture_output=True, check=True, timeout=30)
		subprocess.run([native, "cc", math, "-o", "math"], cwd=root, check=True, timeout=30)
		retained_math = root / ".wovec-cache/default/math/bin/main.c"
		assert private_math.stdout == retained_math.read_bytes()
		assert b"#include <math.h>\n" in private_math.stdout


if __name__ == "__main__":
	main()

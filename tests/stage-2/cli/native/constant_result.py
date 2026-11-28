# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import re
import subprocess


# Read the optimized Wove entry's constant return without running the executable.
def folded_i32_result(clang, runtime, project, standard, flags):
	source = project / ".wovec-cache/default/out/bin/main.c"
	result = subprocess.run([clang, f"-std={standard}", *flags, "-I", project, "-I", runtime,
		"-O2", "-S", "-emit-llvm", source, "-o", "-"],
		cwd=project, capture_output=True, text=True, timeout=30)
	assert result.returncode == 0, (project.name, result.stderr)
	entry = re.search(r"^define [^\n]* @wg[A-Za-z0-9_]*main\([^\n]*\{\n(.*?)^}", result.stdout, re.S | re.M)
	assert entry, (project.name, result.stdout)
	body = entry[1].strip().removeprefix("entry:").strip()
	returned = re.fullmatch(r"ret i32 ([0-9]+)", body)
	assert returned, (project.name, entry[1])
	return int(returned[1])

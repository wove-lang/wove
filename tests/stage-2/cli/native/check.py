# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Run the native compiler and check its declared files, output, and diagnostics."""

import json
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory


# Interpret inline UTF-8 and file-backed data without normalizing newlines.
def phase_bytes(value, fixtures):
	if isinstance(value, str):
		return value.encode("utf-8")
	# File references are relative to the case manifest.
	if isinstance(value, dict) and value.keys() == {"file"}:
		return (fixtures / value["file"]).read_bytes()
	raise ValueError(f"invalid phase data {value!r}")


# Keep spaces, empty strings, and raw argument fixtures as separate operands.
def supplied_arguments(arguments):
	return [value if isinstance(value, str) else bytes.fromhex(value["hex"]) for value in arguments]


# Compare an entire stream against one exact expectation or an explicit list of alternatives.
def check_stream(label, phase, name, actual, expected, fixtures):
	values = expected if isinstance(expected, list) else [expected]
	snapshots = [phase_bytes(value, fixtures) for value in values]
	assert actual in snapshots, (label, phase, name, actual, snapshots)


# Check statuses and streams independently for compilation and execution.
def check_phase(label, phase, result, expected, fixtures, diagnostic="-"):
	assert result.returncode == int(expected["exit"]), (label, phase, result)
	check_stream(label, phase, "stdout", result.stdout, expected.get("stdout", ""), fixtures)
	# Keep JSON diagnostics structural even when the case has no output snapshot.
	if diagnostic != "-":
		assert json.loads(result.stderr) == json.loads(diagnostic), (label, phase, result.stderr)
	# A byte-level snapshot can additionally check the original formatting.
	if diagnostic == "-" or "stderr" in expected:
		check_stream(label, phase, "stderr", result.stderr, expected.get("stderr", ""), fixtures)


# Execute a published program using only the argv and stdin declared for the run phase.
def run_program(root, program, label, fixtures, expected, phase="run"):
	inputs = {"input": phase_bytes(expected["stdin"], fixtures)} if "stdin" in expected else {"stdin": subprocess.DEVNULL}
	arguments = supplied_arguments(expected.get("argv", []))
	result = subprocess.run([program, *arguments], cwd=root, capture_output=True, timeout=30, **inputs)
	check_phase(label, phase, result, expected, fixtures)


# Provide an executable wrapper and preserve a previous binary's bytes for failed builds.
def prepare_tool(root, clang, options, target):
	probe = root / options["probe"]
	probe.chmod(0o700)
	binary = root / target
	previous = binary.read_bytes() if binary.is_file() else None
	# An existing fixture executable must remain runnable after a failed tool invocation.
	if previous is not None:
		binary.chmod(0o700)
	env = {**os.environ, "CLANG": str(probe), "WOVE_CLANG_LOG": str(root / "clang.jsonl"),
		"WOVE_REAL_CLANG": json.dumps(clang)}
	# Exercise the default Clang lookup with a miss before the fixture executable.
	if options.get("search"):
		env.pop("CLANG", None)
		first = root / "empty-path"
		first.mkdir()
		env["PATH"] = os.pathsep.join((str(first), str(probe.parent), os.environ.get("PATH", os.defpath)))
		real_clang = Path(shutil.which(clang[0])).resolve()
		env["WOVE_REAL_CLANG"] = json.dumps([str(real_clang), *clang[1:]])
	env.pop("WOVE_CLANG_STATUS", None)
	env.pop("WOVE_CLANG_LINK_TARGET", None)
	env.pop("WOVE_CLANG_STDOUT", None)
	# Keep fixture-controlled AST outcomes local to this invocation.
	for key in ("ast_status", "ast_output"):
		name = "WOVE_CLANG_" + key.upper()
		env.pop(name, None)
		# Forward supplied outputs verbatim, including an empty capture.
		if key in options:
			env[name] = str(options[key])
	# The fixture controls whether the probe delegates or writes a failed partial stage.
	if "tool_status" in options:
		env["WOVE_CLANG_STATUS"] = str(options["tool_status"])
	# A successful simulated tool can supply a fixture-backed relative symlink.
	if "tool_link_target" in options:
		env["WOVE_CLANG_LINK_TARGET"] = str(root / options["tool_link_target"])
	# A simulated successful tool can report a staged file as a consumed dependency.
	env.pop("WOVE_CLANG_DEPENDENCY", None)
	if "tool_dependency" in options:
		env["WOVE_CLANG_DEPENDENCY"] = options["tool_dependency"]
	env.update(options.get("environment", {}))
	# A fixture can exercise compiler stdout independently of the program's output.
	if "tool_stdout" in options:
		env["WOVE_CLANG_STDOUT"] = options["tool_stdout"]
	return env, previous


# Check header queries and the optional compiler launch in their original order.
def check_tool_calls(root, options):
	log = root / "clang.jsonl"
	calls = [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []
	expected = list(options.get("queries", []))
	# Binary cases also verify their final build invocation and platform link flags.
	if "arguments" in options:
		expected.append(options.get(sys.platform, options["arguments"]))
	assert len(calls) == len(expected), calls
	# Fixture expressions account for the private paths chosen by the driver.
	for arguments, patterns in zip(calls, expected, strict=True):
		assert len(arguments) == len(patterns), arguments
		# Compare every argument without pinning the temporary directory names.
		for actual, pattern in zip(arguments, patterns, strict=True):
			assert re.fullmatch(pattern, actual), (actual, pattern)


# Check retained C and executable publication.
def check_binary(root, options, target, retained, result, patterns, previous, existing_c):
	check_c_artifacts(root, retained, existing_c)
	generated = (root / retained).read_text()
	# Apply the same retained-C pattern checks used by cc fixtures.
	for pattern, count in patterns.items():
		assert len(re.findall(pattern, generated)) == count, pattern
	# Additional outputs use manifest-supplied path and regex assertions.
	for path, checks in options.get("files", {}).items():
		content = (root / path).read_text()
		for pattern in checks.get("matches", []):
			assert re.search(pattern, content), (path, pattern)
		for pattern in checks.get("omits", []):
			assert not re.search(pattern, content), (path, pattern)
	# Toolchain flags can request side outputs, which belong with retained C.
	for stage in root.rglob(".wovec-bin.*"):
		assert stage.parent == (root / retained).parent and not (stage / "program").exists(), stage
	binary = root / target
	# A failed build with an existing target must preserve its executable bytes.
	if result.returncode and previous is not None:
		assert not binary.is_symlink() and binary.read_bytes() == previous
	elif result.returncode == 0:
		assert binary.is_file(), target


# Confirm that emission changed only the declared retained C path.
def check_c_artifacts(root, retained, existing):
	artifacts = {path.relative_to(root).as_posix() for path in root.rglob("*.c") if path.is_file()}
	expected = set(existing)
	if retained != "-":
		expected.add(retained)
	assert artifacts == expected, sorted(artifacts)
	assert all((root / path).read_bytes() == data for path, data in existing.items() if path != retained)


# Capture an entry without following its final symlink.
def entry_snapshot(path):
	# An absent target has no identity to preserve.
	try:
		stamp = path.lstat()
	except (FileNotFoundError, NotADirectoryError):
		return None
	content = None
	# Symlink spellings are part of the state publication must preserve.
	if path.is_symlink():
		content = os.readlink(path)
	elif path.is_file():
		content = path.read_bytes()
	return (stamp.st_dev, stamp.st_ino, stamp.st_mode, content)


# Capture all prior entries without traversing directory symlinks.
def publication_snapshot(root):
	return {path.relative_to(root): entry_snapshot(path) for path in root.rglob("*")}


# Exercise one invocation with the project and files specified by its fixture row.
def check_case(compiler, clang, runtime, fixtures, row):
	label, inputs, argv, status, target, retained, value, diagnostic, *options = row
	patterns = json.loads(options[0]) if options else {}
	flags = json.loads(options[1]) if len(options) > 1 else []
	tool = json.loads(options[2]) if len(options) > 2 else None
	preserve = len(options) > 3 and options[3] == "preserve"
	phases = json.loads(options[4]) if len(options) > 4 else {}
	arguments = supplied_arguments(json.loads(argv))
	bin_command = bool(arguments) and arguments[0] in ("bin", b"bin")
	compile_options = phases.get("compile", {})
	if "exit" in compile_options:
		raise ValueError(f"{label}: the compiler exit belongs in the TSV status column")
	compile_expected = {"exit": int(status), **compile_options}
	run_expected = phases.get("run")
	# An explicit run phase always needs a successful bin invocation with an executable to check.
	if "run" in phases:
		if not isinstance(run_expected, dict) or "exit" not in run_expected:
			raise ValueError(f"{label}: a run phase must declare its exit status")
		if preserve or not bin_command or retained == "-" or int(status) or value != "-":
			raise ValueError(f"{label}: a run phase requires successful bin emission and its own exit expectation")
	# Older successful cases declare an exit-only run after emitting or linking retained C.
	elif value != "-" and int(status) == 0:
		if preserve or retained == "-":
			raise ValueError(f"{label}: a native exit expectation requires retained C")
		run_expected = {"exit": int(value)}
	with TemporaryDirectory(prefix="native-cli-", dir=compiler.parent) as workspace:
		root = Path(workspace)
		# Some Clang cases need the physical path of the isolated workspace.
		physical = str(root.resolve())
		entries = {path.replace("{workspace}", physical): source for path, source in json.loads(inputs).items()}
		# Map fixture files to their supplied names in the command's working directory.
		for destination, source in entries.items():
			path = root / destination
			path.parent.mkdir(parents=True, exist_ok=True)
			# A hex fixture supplies exact raw bytes. Links are created after the regular files.
			if isinstance(source, dict):
				# Stage raw bytes now and validate deferred link descriptors.
				if "hex" in source:
					path.write_bytes(bytes.fromhex(source["hex"]))
				elif not {"hardlink", "symlink"} & source.keys():
					raise ValueError(f"unknown native fixture data for {destination}")
			else:
				shutil.copyfile(fixtures / source, path)
		# Link fixtures use workspace-relative targets, independent of checkout location.
		for destination, source in entries.items():
			# Preserve either shared inode identity or a distinct symbolic spelling.
			if isinstance(source, dict) and "hardlink" in source:
				os.link(root / source["hardlink"], root / destination)
			elif isinstance(source, dict) and "symlink" in source:
				(root / destination).symlink_to(source["symlink"])
		# Existing C paths can also be Wove inputs. A failed command must preserve their bytes.
		existing_c = {path.relative_to(root).as_posix(): path.read_bytes() for path in root.rglob("*.c") if path.is_file()}
		stages = {path.relative_to(root): path.read_bytes() for path in root.rglob("main.c.*") if path.is_file()}
		compiler_argv = [argument.replace("{workspace}", physical) if isinstance(argument, str) else argument
			for argument in arguments]
		preserved = {root / path: (root / path).read_bytes() for path in (tool or {}).get("unchanged", [])}
		env, previous = prepare_tool(root, clang, tool, target) if tool else (None, None)
		before = publication_snapshot(root) if preserve else None
		target_before = entry_snapshot(root / target)
		result = subprocess.run([compiler, *compiler_argv], cwd=root, env=env, capture_output=True, timeout=30)
		check_phase(label, "compile", result, compile_expected, fixtures, diagnostic)
		assert all(path.read_bytes() == original for path, original in preserved.items()), label
		# Check optional probes before inspecting published artifacts.
		if tool:
			check_tool_calls(root, tool)
		# A collision preserves every input and previous artifact with identical entries.
		if preserve:
			assert result.stderr.count(b"\n") == 1, (label, result.stderr)
			after = publication_snapshot(root)
			# The manifest already checked any compiler observation recorded by the wrapper.
			if tool and ("arguments" in tool or "queries" in tool):
				after.pop(Path("clang.jsonl"), None)
			assert after == before, label
			return
		# Bin runs the published program only after a successful compilation.
		if bin_command and (result.returncode == 0 or (tool and "arguments" in tool)):
			check_binary(root, tool or {}, target, retained, result, patterns, previous, existing_c)
			if run_expected is not None:
				run_program(root, root / target, label, fixtures, run_expected)
			# Existing publication regressions separately exercise the preserved old target.
			elif result.returncode and previous is not None and value != "-":
				run_program(root, root / target, label, fixtures, {"exit": int(value)}, "preserved executable")
			return
		# cc leaves the requested target alone even when a file already occupies that path.
		assert entry_snapshot(root / target) == target_before, (label, target)
		check_c_artifacts(root, retained, existing_c)
		assert {path.relative_to(root): path.read_bytes() for path in root.rglob("main.c.*") if path.is_file()} == stages, label
		if retained != "-":
			generated = (root / retained).read_text()
			# The case can count defining symbols without pinning installation-dependent hashes.
			for pattern, count in patterns.items():
				assert len(re.findall(pattern, generated)) == count, (label, pattern)
			# Legacy cc execution cases link retained C after verifying the compiler phase.
			if run_expected is not None:
				program = root / "result"
				subprocess.run([*clang, *flags, "-I", Path("runtime").resolve(), str(root / retained), runtime, "-o", str(program)],
					cwd=root, check=True, timeout=30)
				run_program(root, program, label, fixtures, run_expected)
		elif diagnostic != "-":
			assert result.stderr.count(b"\n") == 1, (label, result.stderr)


# Run each external scenario with the same compiler settings as the suite.
def main():
	compiler, manifest = (Path(value).resolve() for value in sys.argv[1:])
	clang = [*shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang")), "-std=c11",
		*shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))]
	runtime = Path(os.environ.get("WOVE_TEST_RUNTIME", "runtime/wove_runtime.c")).resolve()
	# Relocate the executable so its cases exercise an installed compiler outside the checkout.
	with TemporaryDirectory(prefix="native-wovec-") as install:
		installed = Path(install) / compiler.name
		shutil.copy2(compiler, installed)
		# The manifest supplies paths, argv, artifacts, and diagnostics.
		for line in manifest.read_text(encoding="utf-8").splitlines():
			# Ignore the manifest header and comments.
			if line and not line.startswith("#"):
				check_case(installed, clang, runtime, manifest.parent, line.split("\t"))


if __name__ == "__main__":
	main()

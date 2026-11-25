#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu
python=$1
compiler=$2
fixtures=$(cd "$(dirname "$3")" && pwd -P)
workspace=$(mktemp -d)
trap 'rm -rf "$workspace"' EXIT
cd "$workspace"
workspace=$(pwd -P)
target="build with space=debug/program"
output=$(dirname "$target")
cache="$output/.wovec-cache/debug/$(basename "$target")/bin"
tab=$(printf '\t')

# Record Clang's input with the same wrapper used by the native manifests.
cp "$fixtures/../bin/clang-probe.py" "$workspace/clang probe"
chmod +x "$workspace/clang probe"
CLANG="$workspace/clang probe"
WOVE_CLANG_LOG="$workspace/clang.jsonl"
WOVE_CLANG_CAPTURE_UNIT="$workspace/clang-input"
WOVE_REAL_CLANG=$("$python" -c '
import json
import os
import shlex

print(json.dumps([*shlex.split(os.environ.get("WOVE_TEST_CLANG", "clang")),
    "-std=c11", *shlex.split(os.environ.get("WOVE_TEST_CFLAGS", ""))]))
')
export CLANG WOVE_CLANG_LOG WOVE_CLANG_CAPTURE_UNIT WOVE_REAL_CLANG

# Compare names, inode identities, and file bytes without directory timestamps.
snapshot() (
	cd "$output"
	find . -exec ls -di {} + | LC_ALL=C sort
	find . -type f -exec cksum {} + | LC_ALL=C sort
)

# Resolve each debugger symbol using only the published executable and bundle.
check_sources() {
	[ "$(uname -s)" = Darwin ] || return 0
	binary=$1
	sources=$2
	# The manifest names the symbols and the source text LLDB must display.
	while IFS="$tab" read -r symbol source expected; do
		case "$symbol" in ''|'#'*) continue ;; esac
		lldb --batch -o "target symbols add \"$sources/program.dSYM\"" \
			-o "source list -n $symbol" "$binary" >lldb.out
		grep -F "File: $sources/$source" lldb.out >/dev/null
		grep -F "$expected" lldb.out >/dev/null
	done <"$fixtures/symbols.tsv"
}

retained=
# Run each manifest row against the same output while changing the input fixture.
while IFS="$tab" read -r label input status diagnostic program publication first second tool dependency; do
	case "$label" in ''|'#'*) continue ;; esac
	cp "$fixtures/$input" main.wove
	: >"$WOVE_CLANG_LOG"
	unset WOVE_CLANG_STATUS WOVE_CLANG_DEPENDENCY WOVE_CLANG_LINK_TARGET WOVE_CLANG_STDOUT
	[ "$tool" = - ] || export WOVE_CLANG_STATUS="$tool"
	[ "$dependency" = - ] || export WOVE_CLANG_DEPENDENCY="$dependency"
	set -- "$compiler" bin main.wove --debug -o "$target"
	# Expand a retained-path operand after observing the previous build directory.
	for argument in "$first" "$second"; do
		case "$argument" in
			-) continue ;;
			*'{retained}'*) argument="${argument%%\{retained\}*}$retained${argument#*\{retained\}}" ;;
		esac
		set -- "$@" "$argument"
	done
	# Save the earlier executable before installing a replacement.
	if [ "$publication" = install ] && [ -n "$retained" ]; then
		cp -p "$target" previous-program
		previous=$retained
	fi
	actual=0
	"$@" >compile.out 2>compile.err || actual=$?
	test "$actual" = "$status" || { cat compile.err >&2; exit 1; }
	test ! -s compile.out
	# A failing tool can write its own stderr before the driver's JSON record.
	if [ "$diagnostic" = - ]; then
		test ! -s compile.err
	else
		tail -n 1 compile.err | "$python" -c 'import json, sys; assert json.load(sys.stdin)["code"] == int(sys.argv[1])' "$diagnostic"
	fi
	stage=$("$python" -c '
import json
import sys
from pathlib import Path

arguments = json.loads(Path(sys.argv[1]).read_text().splitlines()[-1])
print(Path(arguments[arguments.index("-o") + 1]).parent)
' "$WOVE_CLANG_LOG")
	test ! -e "$stage"
	# A success installs the captured bytes. A failure preserves the complete snapshot.
	if [ "$publication" = install ]; then
		retained="$workspace/$cache/$(basename "$stage")"
		cmp "$WOVE_CLANG_CAPTURE_UNIT" "$retained/main.c"
		snapshot >before
	else
		tail -n +2 "$retained/main.c" >previous-body.c
		tail -n +2 "$WOVE_CLANG_CAPTURE_UNIT" >rebuilt-body.c
		! cmp -s previous-body.c rebuilt-body.c
		snapshot >after
		cmp before after
	fi
	actual=0
	"$target" >run.out 2>run.err || actual=$?
	test "$actual" = "$program"
	test ! -s run.out
	test ! -s run.err
	check_sources "$target" "$retained"
done <"$fixtures/$(basename "$3")"

# The replacement has its own source path and the older executable still resolves.
test "$retained" != "$previous"
check_sources previous-program "$previous"

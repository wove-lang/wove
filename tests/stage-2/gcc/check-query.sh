#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu
set -f

query=$1
manifest=$2
group=$3
gcc=$4
fixtures=$(cd "$(dirname "$manifest")/fixtures" && pwd)
tab=$(printf '\t')
scratch=$(mktemp -d "${TMPDIR:-/tmp}/wove-gcc-query.XXXXXX")
trap 'rm -rf "$scratch"' 0
mkdir "$scratch/include space" "$scratch/plugin space" "$scratch/query workspace"
cp "$fixtures/query.h" "$scratch/include space/wove_gcc_query.h"
cp "$fixtures/query-tool.sh" "$scratch/gcc tool"
chmod +x "$scratch/gcc tool"

export TMPDIR="$scratch/query workspace"
export WOVE_GCC_TEST_INCLUDE="$scratch/include space"
export WOVE_GCC_TEST_PLUGIN="$scratch/plugin space"
export WOVE_GCC_TEST_CAPTURE="$fixtures/capture.json"
export WOVE_GCC_TEST_INSPECTOR="$fixtures/../../../../stage-2/gcc/inspect.cc"

# Select the supplied GNU compiler for live cases or the controlled transport for mock cases.
while IFS="$tab" read -r mode label tool header kind name error present found bytes rows status target; do
	[ "$mode" = "$group" ] || continue
	# Only the selected tool path varies across discovery, build, and invocation cases.
	case "$tool" in
		gcc) program=$gcc ;;
		missing) program="$scratch/no such GCC" ;;
		*) program="$scratch/gcc tool" ;;
	esac
	export WOVE_GCC_TEST_MODE="$tool"
	# Ask real GCC to load an unavailable plugin in the negative live case.
	case "$target" in
		load-failure) set -- -funsigned-char "-fplugin=$scratch/unavailable.so" ;;
		*) set -- "-f${target}-char" ;;
	esac
	export WOVE_GCC_TEST_TARGET="$1"
	# Wove asserts the query result and original import location.
	"$query" gcc "$program" "$header" "$kind" "$name" "$error" "$present" "$found" \
		"$bytes" "$rows" "$status" "$@" -I "$WOVE_GCC_TEST_INCLUDE" 2>"$scratch/stderr" || {
		failure=$?
		cat "$scratch/stderr" >&2
		printf '%s failed with status %s\n' "$label" "$failure" >&2
		exit 1
	}
	# Check the tool staging tree separately from other compiler temporary files.
	if [ -n "$(find "$TMPDIR" -maxdepth 1 \( -name 'wove-gcc.*' -o -name 'wove-capture.*' \) -print)" ]; then
		printf '%s left a temporary query path\n' "$label" >&2
		exit 1
	fi
done < "$manifest"

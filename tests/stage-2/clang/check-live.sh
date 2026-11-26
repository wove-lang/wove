#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu
set -f

query=$1
manifest=$2
shift 2
fixtures=$(dirname "$manifest")/fixtures
tab=$(printf '\t')
scratch=$(mktemp -d "${TMPDIR:-/tmp}/wove-live-clang.XXXXXX")
trap 'rm -rf "$scratch"' 0
mkdir "$scratch/include space" "$scratch/query workspace"
cp "$fixtures/live_query.h" "$scratch/include space/live_query.h"
cp "$fixtures/live-tool.sh" "$scratch/tool with spaces"
chmod +x "$scratch/tool with spaces"
# Adapt a configured tool command to the query's single-executable interface.
write_clang_wrapper() {
	printf '%s\n' '#!/bin/sh'
	printf '%s' 'exec'
	# Keep every configured word literal, including spaces and single quotes.
	for argument do
		quoted=$(printf '%s.' "$argument" | sed "s/'/'\\\\''/g")
		printf " '%s'" "${quoted%.}"
	done
	printf '%s\n' ' "$@"'
}

# Pass a plain executable directly. Wrap only commands with fixed arguments.
if [ "$#" -eq 1 ]; then
	clang=$1
else
	clang="$scratch/configured clang"
	write_clang_wrapper "$@" > "$clang"
	chmod +x "$clang"
fi

export TMPDIR="$scratch/query workspace"
export WOVE_CLANG_FIXTURE_CLANG="$clang"

# Relay each case's arguments and report the failing Wove assertion.
while IFS="$tab" read -r label tool header kind name error present found bytes rows status options; do
	# Blank and comment lines carry no case data.
	case "$label" in
		""|\#*) continue ;;
	esac
	# Keep the launch failure distinct from a working executable that exits nonzero.
	case "$tool" in
		clang) program=$clang ;;
		missing) program="$scratch/no such tool" ;;
		*) program="$scratch/tool with spaces" ;;
	esac
	set --
	# The spaced include directory must stay in a single argv element.
	case "$options" in
		target|target-c17) set -- "--target=i386-unknown-linux-gnu" -I "$scratch/include space" ;;
	esac
	[ "$options" != target-c17 ] || set -- "$@" -std=c17
	# Wove owns every result assertion. The shell supplies tools and fixture inputs.
	WOVE_CLANG_FIXTURE_OUTPUT="$tool" "$query" "$program" "$header" "$kind" "$name" \
		"$error" "$present" "$found" "$bytes" "$rows" "$status" "$@" || {
		failure=$?
		printf '%s failed with status %s\n' "$label" "$failure" >&2
		exit 1
	}
	# All normal and error exits remove the private source and temporary workspace.
	if [ -n "$(find "$TMPDIR" -maxdepth 1 \( -name 'wove-clang.*' -o -name 'wove-capture.*' \) -print)" ]; then
		printf '%s left a temporary query path\n' "$label" >&2
		exit 1
	fi
done < "$manifest"

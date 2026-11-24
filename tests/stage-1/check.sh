#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

python=$1
build=$2
manifest=$3
fixtures=$(dirname "$manifest")

# Check the process result even when the Wove program returns a nonzero value.
check_status() {
	expected=$1
	program=$2
	status=0
	"$program" || status=$?
	# Report fixture failures with both statuses.
	if [ "$status" != "$expected" ]; then
		printf '%s returned %s, expected %s\n' "$program" "$status" "$expected" >&2
		exit 1
	fi
}

# A source rejection must give the expected line and emit no C.
check_rejected() {
	fixture=$1
	line=$2
	status=0
	"$python" stage-1/wovec.py "$fixture" > "$build/rejected.c" 2> "$build/rejected.stderr" || status=$?
	# A rejected source must exit with the seed error status and leave stdout empty.
	if [ "$status" -ne 1 ] || [ -s "$build/rejected.c" ]; then
		printf '%s did not reject cleanly\n' "$fixture" >&2
		exit 1
	fi
	printf '%s:%s\n' "$fixture" "$line" > "$build/expected.stderr"
	diff -u "$build/expected.stderr" "$build/rejected.stderr"
}

# Evaluate the fixture list supplied by Make.
while read -r mode source expected extra <&3 || [ -n "${mode:-}" ]; do
	# Allow blank lines and comments between cases.
	case "$mode" in
		""|\#*) continue ;;
	esac
	# Every case names one source and one expected result.
	if [ -z "$source" ] || [ -z "$expected" ] || [ -n "$extra" ]; then
		printf 'invalid case in %s: %s %s %s %s\n' "$manifest" "$mode" "$source" "$expected" "$extra" >&2
		exit 1
	fi

	# Match the decimal spelling emitted by the process or source reporter.
	case "$expected" in
		*[!0123456789]*|0?*)
			printf 'invalid expected result in %s for %s: %s\n' "$manifest" "$source" "$expected" >&2
			exit 1
			;;
	esac

	program="$build/${source%.wove}"
	# Run the requested check and compare the result from the manifest.
	case "$mode" in
		# Compare the fixture exit status with the ordinary runtime.
		run)
			check_status "$expected" "$program"
			;;
		# Repeat the fixture with the allocation probe linked in.
		tracked)
			check_status "$expected" "$program"
			check_status "$expected" "$program.tracked"
			;;
		# Check that the source fails on the specified line.
		reject)
			check_rejected "$fixtures/$source" "$expected"
			;;
		# Reject unknown modes so every listed fixture is exercised.
		*)
			printf 'unknown test mode in %s: %s\n' "$manifest" "$mode" >&2
			exit 1
			;;
	esac
done 3< "$manifest"

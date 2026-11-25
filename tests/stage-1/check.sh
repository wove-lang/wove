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
	shift 2
	status=0
	"$program" "$@" || status=$?
	# Report fixture failures with both statuses.
	if [ "$status" != "$expected" ]; then
		printf '%s returned %s, expected %s\n' "$program" "$status" "$expected" >&2
		exit 1
	fi
}

# Read one argument after each fixed TSV marker. A bare marker represents an empty argument.
check_argv_status() {
	expected=$1
	program=$2
	case_file=$3
	tab=$(printf '\t')
	set --
	# Keep the values opaque to the shell and exclude the fixture's copyright comments.
	while IFS= read -r row || [ -n "$row" ]; do
		case "$row" in
			""|\#*) continue ;;
			arg) set -- "$@" "" ;;
			arg"$tab"*) set -- "$@" "${row#arg"$tab"}" ;;
			*) printf 'invalid argument row in %s\n' "$case_file" >&2; exit 1 ;;
		esac
	done < "$case_file"
	check_status "$expected" "$program" "$@"
}

# A source rejection must give the expected line and emit no C.
check_rejected() {
	fixture=$1
	line=$2
	origin=$3
	status=0
	"$python" stage-1/wovec.py "$fixture" > "$build/rejected.c" 2> "$build/rejected.stderr" || status=$?
	# A rejected source must exit with the seed error status and leave stdout empty.
	if [ "$status" -ne 1 ] || [ -s "$build/rejected.c" ]; then
		printf '%s did not reject cleanly\n' "$fixture" >&2
		exit 1
	fi
	printf '%s:%s\n' "$origin" "$line" > "$build/expected.stderr"
	diff -u "$build/expected.stderr" "$build/rejected.stderr"
}

# A runtime failure must report the expected trap at the specified line.
check_trap() {
	origin=$1
	line=$2
	program=$3
	diagnostic=$4
	status=0
	"$program" 2> "$build/trap.stderr" || status=$?
	# A successful exit cannot satisfy a runtime trap expectation.
	if [ "$status" -eq 0 ]; then
		printf '%s completed, expected a runtime trap\n' "$program" >&2
		exit 1
	fi
	printf '%s:%s:1: runtime error: %s\n' "$origin" "$line" "$diagnostic" > "$build/expected.stderr"
	diff -u "$build/expected.stderr" "$build/trap.stderr"
}

# Evaluate the fixture list supplied by Make.
while read -r mode source expected origin extra <&3 || [ -n "${mode:-}" ]; do
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
	# Diagnostic cases can point to a provider reached from the entry fixture.
	location="$fixtures/${origin:-$source}"
	# Exit-status cases do not have a source location to compare.
	case "$mode:$origin" in
		run:?*|tracked:?*)
			printf 'unexpected diagnostic source in %s for %s\n' "$manifest" "$source" >&2
			exit 1
			;;
	esac
	# Run the requested check and compare the result from the manifest.
	case "$mode" in
		# Compare the fixture exit status with the ordinary runtime.
		run)
			check_status "$expected" "$program"
			;;
		# The fourth column names the fixture supplying the exact user argv.
		run-argv)
			check_argv_status "$expected" "$program" "$fixtures/$origin"
			;;
		# Repeat the fixture with the allocation probe linked in.
		tracked)
			check_status "$expected" "$program"
			check_status "$expected" "$program.tracked"
			;;
		# Check that the source fails on the specified line.
		reject)
			check_rejected "$fixtures/$source" "$expected" "$location"
			;;
		# Check the exact diagnostic from a runtime bounds trap.
		bounds-trap)
			check_trap "$location" "$expected" "$program" "slice index out of bounds"
			;;
		# Check the exact diagnostic from a failed integer conversion.
		conversion-trap)
			check_trap "$location" "$expected" "$program" "integer conversion out of range"
			;;
		# Check a range was rejected before the helper ran.
		range-trap)
			check_trap "$location" "$expected" "$program" "slice range out of bounds"
			;;
		# Invalid shift counts report the shift operation's original line.
		shift-trap)
			check_trap "$location" "$expected" "$program" "shift count out of range"
			;;
		# Reject unknown modes so every listed fixture is exercised.
		*)
			printf 'unknown test mode in %s: %s\n' "$manifest" "$mode" >&2
			exit 1
			;;
	esac
done 3< "$manifest"

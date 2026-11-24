#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu
set -f

probe=$1
template=$2
manifest=$3
shift 3
fixtures=$(dirname "$manifest")
tab=$(printf '\t')
scratch=$(mktemp -d "${TMPDIR:-/tmp}/wove-clang-cases.XXXXXX")
trap 'rm -rf "$scratch"' 0

# Report the case and the Wove assertion or C compiler exit status.
fail() {
	printf '%s failed with status %s\n' "$case_name" "$1" >&2
	exit 1
}

# Route the supplied input and expected bytes through Wove before compiling its C.
check_source() {
	fields=$1
	shift
	IFS="$tab" read -r source_template header extra literal first second size error presence <<EOF
$fields
EOF
	# An explicitly named fixture can supply the template instead.
	if [ "$source_template" = "-" ]; then
		source_template=$template
	else
		source_template="$fixtures/$source_template"
	fi
	# Escapes allow an invalid header with control bytes to fit in the manifest.
	if [ "$header" = "-" ]; then
		header=
	else
		header=$(printf '%b' "$header")
	fi
	# The source fixture's extra C is read byte for byte by the transport.
	if [ "$extra" != "-" ]; then
		extra="$fixtures/$extra"
	fi
	"$probe" source "$source_template" "$header" "$extra" "$literal" "$first" "$second" \
		"$size" "$error" > "$scratch/probe.c" || fail "$?"
	# Only a case supplying the expected presence value produces compilable C.
	if [ "$presence" != "-" ]; then
		"$@" -std=c11 -Werror -I "$fixtures/fixtures" "-DWOVE_EXPECT_PRESENT=$presence" \
			-fsyntax-only "$scratch/probe.c" || fail "$?"
	fi
}

# Turn the tab-separated case into the Wove fixture's transport arguments.
check_index() {
	IFS="$tab"
	set -- $1
	unset IFS
	input="$fixtures/$1"
	shift
	"$probe" index "$input" "$@" || fail "$?"
}

# Forward each manifest row as arguments without interpreting Wove behavior here.
while IFS="$tab" read -r mode case_name arguments; do
	# Blank and comment lines have no case arguments.
	case "$mode" in
		""|\#*) continue ;;
	esac
	# Select the corresponding transport mode.
	case "$mode" in
		source)
			check_source "$arguments" "$@"
			;;
		index)
			check_index "$arguments"
			;;
		*)
			printf 'Unknown fixture mode in %s\n' "$manifest" >&2
			exit 1
			;;
	esac
done < "$manifest"

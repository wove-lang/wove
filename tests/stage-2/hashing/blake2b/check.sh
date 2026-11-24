#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

probe=$1
cases=$2
tab=$(printf '\t')

# Forward fixture fields without computing any expected hashes in the runner.
while IFS="$tab" read -r mode name input split early_raw early_text final_raw final_text; do
	# Empty and comment lines have no data fields.
	case "$mode" in
		""|\#*) continue ;;
		raw) set -- "$final_raw" "$final_text" ;;
		fields) set -- ;;
		*) printf 'Unknown BLAKE2b fixture mode %s\n' "$mode" >&2; exit 1 ;;
	esac
	# The exit code identifies the Wove assertion that failed.
	if "$probe" "$mode" "$input" "$split" "$early_raw" "$early_text" "$@"; then
		continue
	else
		status=$?
		printf '%s failed with status %s\n' "$name" "$status" >&2
		exit 1
	fi
done < "$cases"

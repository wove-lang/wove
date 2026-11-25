#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

generated=$1
expectation=$2
directory=$(CDPATH= cd -P "$(dirname "$expectation")" && pwd)

# Collect the include prefix and make project paths relative to the expectation.
actual=$(WOVE_INCLUDE_FIXTURES="$directory" awk '
	BEGIN { project = "#include \"" ENVIRON["WOVE_INCLUDE_FIXTURES"] "/" }
	/^#include / {
		if (ended) exit 1
		if (index($0, project) == 1) $0 = "#include \"" substr($0, length(project) + 1)
		print
		next
	}
	!ended {
		if (NR == 1 || $0 != "") exit 1
		ended = 1
	}
	END { if (!ended) exit 1 }
' "$generated") || {
	printf '%s does not start with a contiguous include block\n' "$generated" >&2
	exit 1
}
expected=$(sed -n '/^#include /p' "$expectation")

# Show both ordered lists when the generated includes differ.
if [ "$actual" != "$expected" ]; then
	printf '%s includes\n%s\nExpected from %s\n%s\n' "$generated" "$actual" "$expectation" "$expected" >&2
	exit 1
fi

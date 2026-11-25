#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

# Write the last commit's UTC author timestamp and revision for compile-time embedding.
set -eu
export LC_ALL=C
root=$(CDPATH= cd -P -- "$(dirname -- "$0")/.." && pwd -P)

# Report invalid metadata before changing the embedded version asset.
fail() {
	printf 'build-info: %s\n' "$1" >&2
	exit 1
}

# Accept the optional output path used by Make and standalone bootstrap builds.
case $# in
	0) output="$root/build/version.txt" ;;
	1) output=$1 ;;
	*) fail 'usage: sh tools/build-info.sh [output]' ;;
esac

# Explicit CI metadata also permits builds from archives without Git metadata.
if [ "${WOVEC_BUILD_VERSION+x}" = x ]; then
	version=$WOVEC_BUILD_VERSION
else
	missing='Git metadata is unavailable for the source root; set WOVEC_BUILD_VERSION to YYMMDD.HHMM-SHA'
	# Pin Git to this root's metadata directory or linked worktree file.
	metadata=$(
		unset GIT_COMMON_DIR
		TZ=UTC git -C "$root" --git-dir=.git --work-tree=. log -1 --no-show-signature \
			--date=format-local:%y%m%d.%H%M --format=%ad-%H 2>/dev/null
	) || fail "$missing"
	revision=${metadata##*-}
	version="${metadata%-*}-$(printf '%.7s' "$revision")"
fi

# Require the fixed version shape before checking calendar and clock ranges.
case $version in
	[0-9][0-9][0-9][0-9][0-9][0-9].[0-9][0-9][0-9][0-9]-[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]) ;;
	*) fail 'WOVEC_BUILD_VERSION must use YYMMDD.HHMM-SHA' ;;
esac

# Two-digit years use 2000–2068 and 1969–1999; every represented leap year is divisible by four.
printf '%s\n' "$version" | awk -F '[.-]' '
{
	year = substr($1, 1, 2) + 0
	month = substr($1, 3, 2) + 0
	day = substr($1, 5, 2) + 0
	hour = substr($2, 1, 2) + 0
	minute = substr($2, 3, 2) + 0
	days = 31
	# April, June, September, and November have thirty days.
	if (month == 4 || month == 6 || month == 9 || month == 11) {
		days = 30
	}
	# February includes the leap day only in leap years.
	if (month == 2) {
		days = 28 + (year % 4 == 0)
	}
	exit !(month >= 1 && month <= 12 && day >= 1 && day <= days && hour < 24 && minute < 60)
}' || fail "WOVEC_BUILD_VERSION has an invalid date or time"

# Preserve the timestamp when the bytes match so unchanged builds remain up to date.
if printf '%s\n' "$version" | cmp -s "$output" -; then
	exit 0
fi

# Publish the completed line as a sibling so readers never see a partial asset.
[ ! -d "$output" ] || fail 'output path is a directory'
mkdir -p -- "$(dirname -- "$output")"
temporary=$(mktemp -- "$output.XXXXXX")
trap 'rm -f -- "$temporary"' 0
trap 'exit 1' HUP INT TERM
printf '%s\n' "$version" > "$temporary"
mv -f -- "$temporary" "$output"

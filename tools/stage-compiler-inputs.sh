#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

destination=$1
version=$2
mkdir -p "$(dirname "$destination")"
work=$(mktemp -d "$destination.tmp.XXXXXX")

# Restore the previous input tree if installing the new tree fails.
cleanup() {
	# A missing destination means the new tree was never installed.
	if [ -e "$work/previous" ] && [ ! -e "$destination" ]; then
		mv "$work/previous" "$destination"
	fi
	rm -rf -- "$work"
}

trap cleanup 0
trap 'exit 1' HUP INT TERM

mkdir -p "$work/next/build"
cp -R stage-2 runtime "$work/next/"
cp "$version" "$work/next/build/version.txt"
: > "$work/next/.ready"

# Keep the previous tree until its complete replacement is ready.
if [ -e "$destination" ]; then
	mv "$destination" "$work/previous"
fi
mv "$work/next" "$destination"

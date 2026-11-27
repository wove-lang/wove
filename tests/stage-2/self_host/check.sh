#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

make=$1
work=$(mktemp -d "$2/self-host-check.XXXXXX")
version=$(cat "$3")
trap 'rm -rf -- "$work"' 0
trap 'exit 1' HUP INT TERM

project="$work/project"
mkdir "$project"
cp -R Makefile stage-1 stage-2 runtime tools wovec.jsonc "$project/"
# Make reads the seed manifest while loading targets, including for an all build.
mkdir -p "$project/tests/stage-1"
cp tests/stage-1/cases.tsv "$project/tests/stage-1/"

output=custom-output
snapshot="$output/self-host-source"
native_import=stage-2/text/equal.wove
seed_import=stage-2/driver/bootstrap/entry.wove
asset=stage-2/clang/templates/probe_header.tmpl.h
products='stage-1/wovec.c stage-1/wovec self-host-source/.ready stage-2/wovec.c stage-2/wovec wovec'

# Keep the version and output paths fixed while changing only source input bytes.
build() {
	"$make" -s "BUILD_DIR=$output" "WOVEC_BUILD_VERSION=$version" "$@"
}

# Check the copied inputs and byte parity between the two native generations.
check_snapshot() {
	diff -r stage-2 "$snapshot/stage-2"
	diff -r runtime "$snapshot/runtime"
	cmp "$output/stage-2/wovec.c" "$output/.wovec-cache/default/wovec/bin/main.c"
}

# Record both artifact bytes and timestamps before a rebuild.
save_products() {
	# Preserve relative product paths in the comparison tree.
	for product in $products; do
		mkdir -p "$work/before/$(dirname "$product")"
		cp -p "$output/$product" "$work/before/$product"
	done
}

# Reject any artifact change during an unchanged rerun.
check_unchanged() {
	# Compare freshness in both directions to detect any timestamp change.
	for product in $products; do
		before="$work/before/$product"
		after="$output/$product"
		cmp "$before" "$after"
		test -z "$(find "$before" -newer "$after")"
		test -z "$(find "$after" -newer "$before")"
	done
}

cd "$project"
build all
check_snapshot
save_products
build all
check_unchanged

# Separate the source edit from the last build even on Make with whole-second times.
sleep 1
printf '\n' >> "$native_import"
build all
check_snapshot
# A native-only transitive edit must restage the input and rebuild both generations.
for product in self-host-source/.ready stage-2/wovec.c stage-2/wovec wovec; do
	test -n "$(find "$output/$product" -newer "$work/before/$product")"
done

save_products
sleep 1
printf '\n' >> "$seed_import"
printf '\n' >> "$asset"
build all
check_snapshot
# Both the seed generation and the staged native generations should be fresh.
for product in $products; do
	test -n "$(find "$output/$product" -newer "$work/before/$product")"
done
save_products
build all
check_unchanged

# A new file with an old timestamp relies on the source directory prerequisite.
added=stage-2/incremental-input
sleep 1
cp "$asset" "$added"
touch -t 200001010000 "$added"
build "$snapshot/.ready"
cmp "$added" "$snapshot/$added"

# Removing the input also invalidates the snapshot via its parent directory.
sleep 1
rm "$added"
build "$snapshot/.ready"
test ! -e "$snapshot/$added"

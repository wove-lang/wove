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
# The seed uses the existing Wove emitter entry and its C interface.
mkdir -p "$project/tests/stage-2"
cp tests/stage-2/source_scan.wove tests/stage-2/source_scan.probe.c "$project/tests/stage-2/"
# Keep the native fixture entry beside the archived test inputs.
numbered_entry=tests/stage-2/numbered_units/bridges/check.wove
mkdir -p "$project/$(dirname "$numbered_entry")"
cp "$numbered_entry" "$project/$numbered_entry"

output=custom-output
snapshot="$output/self-host-source"
numbered="$output/stage-2/numbered-headers"
native_import=stage-2/clang/replay.wove
seed_import=stage-2/source/scan.wove
asset=stage-2/clang/templates/probe_header.tmpl.h
native_unit=stage-2/.wovec-cache/default/wovec/bin/main.c
products="stage-1/wovec.c stage-1/wovec self-host-source/.ready $native_unit stage-2/wovec wovec stage-2/numbered-headers"

# Keep the version and output paths fixed while changing only source input bytes.
build() {
	"$make" -s "BUILD_DIR=$output" "WOVEC_BUILD_VERSION=$version" "$@"
}

# Check the copied inputs and byte parity between the two native generations.
check_snapshot() {
	diff -r stage-2 "$snapshot/stage-2"
	diff -r runtime "$snapshot/runtime"
	cmp "$output/$native_unit" "$output/.wovec-cache/default/wovec/bin/main.c"
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
build all "$numbered"
check_snapshot
save_products
build all "$numbered"
check_unchanged

# Separate the source edit from the last build even on Make with whole-second times.
sleep 1
printf '\n' >> "$native_import"
build all "$numbered"
check_snapshot
# A transitive native edit must rebuild all compiler generations and the numbered fixture.
for product in $products; do
	test -n "$(find "$output/$product" -newer "$work/before/$product")"
done

save_products
sleep 1
printf '\n' >> "$seed_import"
printf '\n' >> "$asset"
build all "$numbered"
check_snapshot
# Both the seed generation and the staged native generations should be fresh.
for product in $products; do
	test -n "$(find "$output/$product" -newer "$work/before/$product")"
done
save_products
build all "$numbered"
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

# Add a dependency to an existing import without changing Make's source inventory.
scanner_unit="$output/stage-2/source_scan.c"
scanner_deps="$scanner_unit.d"
scanner_provider='tests/stage-2/new/nested/scanner provider.wove'
scanner_importer=stage-2/source/expectations.wove
absolute=$(pwd -P)

# Insert one import into the scanner's already reachable module in the disposable tree.
add_scanner_import() {
	sed "/(use wove-file \".\\/scan.wove\"/i\\
	(use wove-file \"../../$1\" (only))
" "$scanner_importer" > "$work/scanner-importer"
	cp "$work/scanner-importer" "$scanner_importer"
}

# A missing active path must reach the seed and leave both previous products intact.
check_missing_scanner_import() {
	# The same sidecar must work with an absolute BUILD_DIR.
	if build "BUILD_DIR=$absolute/$output" "$absolute/$scanner_unit" > "$work/scanner-failure" 2>&1; then
		printf 'scanner accepted a missing transitive import\n' >&2
		exit 1
	fi
	grep -q "^$scanner_importer:[0-9]" "$work/scanner-failure"
	cmp "$scanner_unit" "$work/scanner-before"
	cmp "$scanner_deps" "$work/scanner-deps-before"
	test -z "$(find "$scanner_unit" -newer "$work/scanner-before")"
	test -z "$(find "$scanner_deps" -newer "$work/scanner-deps-before")"
}

# A build made before dependency files existed can recreate the sidecar.
mv "$scanner_deps" "$work/old-scanner-deps"
build "$scanner_unit"
cmp "$scanner_deps" "$work/old-scanner-deps"

mkdir -p "$(dirname "$scanner_provider")"
cp stage-2/c/identifiers.wove "$scanner_provider"
sleep 1
add_scanner_import "$scanner_provider"
build "$scanner_unit"
grep -Fq 'tests/stage-2/new/nested/scanner\ provider.wove' "$scanner_deps"

# Editing the discovered provider must regenerate C through the saved dependency.
cp -p "$scanner_unit" "$work/scanner-before"
sleep 1
cp stage-2/text/boolean.wove "$scanner_provider"
build "$scanner_unit"
test -n "$(find "$scanner_unit" -newer "$work/scanner-before")"
! cmp -s "$scanner_unit" "$work/scanner-before"

# An unchanged scan keeps both products, and a missing import preserves them on failure.
cp -p "$scanner_unit" "$work/scanner-before"
cp -p "$scanner_deps" "$work/scanner-deps-before"
build "$scanner_unit"
mv "$scanner_provider" "$work/scanner-provider"
check_missing_scanner_import

# Deduplicating the linked provider in C must still track both active import paths.
scanner_alias=tests/stage-2/new/nested/alias.wove
mv "$work/scanner-provider" "$scanner_provider"
ln "$scanner_provider" "$scanner_alias"
sleep 1
add_scanner_import "$scanner_alias"
build "$scanner_unit"
grep -Fq "$scanner_alias" "$scanner_deps"
cp -p "$scanner_unit" "$work/scanner-before"
cp -p "$scanner_deps" "$work/scanner-deps-before"
mv "$scanner_alias" "$work/scanner-hardlink"
check_missing_scanner_import

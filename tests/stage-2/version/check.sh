#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

make=$1
build=$2
compiler=$3
version=$4
manifest=$5
seed=$6
generator=tools/build-info.sh
work=$(mktemp -d "$build/version-check.XXXXXX")
trap 'rm -rf -- "$work"' 0
trap 'exit 1' HUP INT TERM
unset WOVEC_BUILD_VERSION GIT_DIR GIT_WORK_TREE GIT_COMMON_DIR GIT_INDEX_FILE
unset GIT_OBJECT_DIRECTORY GIT_ALTERNATE_OBJECT_DIRECTORIES

# Compare the complete line, including its one terminating newline.
check_line() {
	printf '%s\n' "$1" > "$work/expected"
	diff -u "$work/expected" "$2"
}

# Check both directions so changing a modification time either way fails.
same_timestamp() {
	test -f "$1"
	test -f "$2"
	test -z "$(find "$1" -newer "$2")"
	test -z "$(find "$2" -newer "$1")"
}

# Check the archive's two aliases byte-for-byte against the generated line.
check_compiler() {
	# Both invocations must write the same output and have no diagnostics.
	for alias in version --version; do
		"$1" "$alias" > "$work/stdout" 2> "$work/stderr"
		test ! -s "$work/stderr"
		check_line "wovec 0.0.$2" "$work/stdout"
	done
}

# Build the real compiler under a separate build directory, reusing the checked seed.
build_compiler() {
	"$make" -s stage-2 "BUILD_DIR=$work/native" "STAGE1=$seed" "WOVEC_BUILD_VERSION=$1"
}

# Change only the version and exercise the real Make targets under equal timestamps.
check_rebuild() {
	native="$work/native/stage-2/wovec"
	emitted="$native.c"
	native_asset="$work/native/version.txt"
	build_compiler "$1"
	check_compiler "$native" "$1"

	touch -t 200001010000 "$native_asset"
	cp -p "$native_asset" "$work/asset-before"
	cp -p "$emitted" "$work/source-before"
	cp -p "$native" "$work/compiler-before"
	build_compiler "$1"
	same_timestamp "$native_asset" "$work/asset-before"
	same_timestamp "$emitted" "$work/source-before"
	same_timestamp "$native" "$work/compiler-before"

	WOVEC_BUILD_VERSION=$2 sh "$generator" "$native_asset"
	touch -r "$native" "$native_asset" "$emitted"
	build_compiler "$2"
	check_compiler "$native" "$2"

	mv "$native_asset" "$work/version.moved"
	test ! -e "$native_asset"
	mkdir "$work/unconfigured"
	(cd "$work/unconfigured" && WOVEC_BUILD_VERSION=$1 check_compiler "$native" "$2")
}

check_compiler "$compiler" "$(cat "$version")"
project="$work/project"
mkdir -p "$project/tools"
cp "$generator" "$project/tools/"
tool="$project/tools/build-info.sh"
asset="$work/generated/version.txt"

# Each manifest row supplies metadata inputs or an isolated rebuild scenario.
while IFS="$(printf '\t')" read -r kind first second third; do
	# Dispatch the same generator or build recipes for every fixture case.
	case "$kind" in
		""|\#*) continue ;;
		git)
			git -C "$project" init -q
			GIT_AUTHOR_DATE=$first GIT_COMMITTER_DATE=$second git -C "$project" \
				-c user.name='Wove test' -c user.email=test@example.invalid \
				-c commit.gpgsign=false -c core.hooksPath=/dev/null commit --allow-empty -qm version
			revision=$(git -C "$project" rev-parse HEAD)
			TZ=Pacific/Honolulu GIT_DIR="$work/no-git" sh "$tool"
			check_line "$third-$(printf '%.7s' "$revision")" "$project/build/version.txt"
			;;
		worktree)
			linked="$work/linked checkout"
			git -C "$project" -c core.hooksPath=/dev/null worktree add -q --detach "$linked" HEAD
			test -f "$linked/.git"
			mkdir -p "$linked/tools"
			cp "$generator" "$linked/tools/"
			sh "$linked/tools/build-info.sh"
			cmp "$project/build/version.txt" "$linked/build/version.txt"
			;;
		nested)
			archive="$project/source archive"
			mkdir -p "$archive/tools"
			cp "$generator" "$archive/tools/"
			# Check normal Git discovery and an explicitly selected outer Git directory.
			for outer in "" "$project/.git"; do
				# An unpacked source tree must request an override in both environments.
				if (if [ -n "$outer" ]; then export GIT_DIR="$outer"; fi
					sh "$archive/tools/build-info.sh") > "$work/stdout" 2> "$work/stderr"; then
					exit 1
				fi
				grep -q 'set WOVEC_BUILD_VERSION' "$work/stderr"
				test ! -e "$archive/build/version.txt"
			done
			WOVEC_BUILD_VERSION=$first sh "$archive/tools/build-info.sh"
			check_line "$first" "$archive/build/version.txt"
			;;
		override)
			GIT_DIR="$work/no-git" WOVEC_BUILD_VERSION=$first sh "$tool" "$asset"
			check_line "$first" "$asset"
			;;
		reject)
			cp "$asset" "$work/previous"
			# Reject bad overrides before changing the generated file.
			if WOVEC_BUILD_VERSION=$first sh "$tool" "$asset" > "$work/stdout" 2> "$work/stderr"; then
				printf 'build-info accepted an invalid override: %s\n' "$first" >&2
				exit 1
			fi
			grep -q WOVEC_BUILD_VERSION "$work/stderr"
			cmp "$asset" "$work/previous"
			;;
		switch)
			check_rebuild "$first" "$second"
			;;
		*)
			printf 'unknown build-info fixture kind: %s\n' "$kind" >&2
			exit 1
			;;
	esac
done < "$manifest"

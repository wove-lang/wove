#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

# Discovery must use the chosen tool's own installed plugin directory.
if [ "$#" -eq 1 ] && [ "$1" = -print-file-name=plugin ]; then
	case "$WOVE_GCC_TEST_MODE" in
		bare-path) printf '%s\n' plugin ;;
		multiple-paths) printf '%s\n' "$WOVE_GCC_TEST_PLUGIN" /another/plugin ;;
		*) printf '%s\r\n' "$WOVE_GCC_TEST_PLUGIN" ;;
	esac
	exit 0
fi

include=
source=
plugin=
output=
target=
syntax=

# Keep the probe's input, target, and paths as separately supplied argv words.
while [ "$#" -gt 0 ]; do
	case "$1" in
		-I) include=$2; shift ;;
		-I*) include=${1#-I} ;;
		-o) output=$2; shift ;;
		-fplugin=*) plugin=${1#-fplugin=} ;;
		-funsigned-char|-fsigned-char) target=$1 ;;
		-fsyntax-only) syntax=$1 ;;
		*/inspect.cc|*/probe.c) source=$1 ;;
	esac
	shift
done

# Check the published embedded source and the compiler-specific include path.
if [ -n "$output" ]; then
	[ "$include" = "$WOVE_GCC_TEST_PLUGIN/include" ] && [ -z "$target" ] &&
		cmp -s "$source" "$WOVE_GCC_TEST_INSPECTOR" || exit 71
	case "$WOVE_GCC_TEST_MODE" in
		build-failure) exit 19 ;;
		no-object) exit 0 ;;
		symlink-object) ln -s "$WOVE_GCC_TEST_INSPECTOR" "$output" ;;
		*) printf '%s\n' 'fixture inspector artifact' > "$output" ;;
	esac
	exit 0
fi

# An inspection must load the freshly built plugin and receive the caller's flags.
[ -f "$plugin" ] && [ "$include" = "$WOVE_GCC_TEST_INCLUDE" ] &&
	[ "$syntax" = -fsyntax-only ] && [ "$target" = "$WOVE_GCC_TEST_TARGET" ] &&
	grep -Fq '#if __has_include(' "$source" || exit 72

# Deliver distinct plugin failure, successful absence, and AST output cases.
case "$WOVE_GCC_TEST_MODE" in
	load-failure) exit 26 ;;
	malformed) printf '%s' '{"inner":[' ;;
	absent) printf '%s\n' '{"inner":[{"kind":"EnumDecl","name":"wvpr_c_probe","inner":[{"name":"wvpr_header_present","inner":[{"value":"0"}]}]}]}' ;;
	*) cat "$WOVE_GCC_TEST_CAPTURE" ;;
esac

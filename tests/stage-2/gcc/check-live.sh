#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

probe=$1
template=$2
inspector=$3
gcc=$4
cxx=$5
fixtures=$(dirname "$0")
runner="$fixtures/../../support/ast/check.sh"

plugin_dir=$("$gcc" -print-file-name=plugin)
# GCC must supply headers from the same build that will load the plugin.
if [ ! -r "$plugin_dir/include/gcc-plugin.h" ]; then
	printf 'GCC plugin headers missing for %s at %s/include\n' "$gcc" "$plugin_dir" >&2
	exit 1
fi

scratch=$(mktemp -d "${TMPDIR:-/tmp}/wove-gcc-cases.XXXXXX")
trap 'rm -rf "$scratch"' 0

# Leave GCC's internal symbols to be resolved when it loads the plugin.
case "$(uname -s)" in
	Darwin)
		set -- -bundle -Wl,-undefined,dynamic_lookup
		;;
	*)
		set -- -shared
		;;
esac
"$cxx" -std=c++11 -O2 -fPIC -fno-rtti -fno-exceptions -I "$plugin_dir/include" "$@" \
	"$inspector" -o "$scratch/inspect.so"

sh "$runner" "$probe" "$template" "$fixtures/source.tsv" "$gcc" \
	"-fplugin=$scratch/inspect.so" > "$scratch/probe.json"
cp "$fixtures/live.tsv" "$scratch/cases.tsv"
sh "$runner" "$probe" "$template" "$scratch/cases.tsv"

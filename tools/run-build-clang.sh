#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

# Parse the configured Make command with shell quoting and append the native argv verbatim.
exec sh -c "$WOVE_BUILD_CLANG \"\$@\"" wove-build-clang "$@"

#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

# Supply bounded, deterministic tool outputs without interpreting any Clang argument.
case "$WOVE_CLANG_FIXTURE_OUTPUT" in
	malformed) printf '%s' '{"inner": [' ;;
	no-fact) printf '%s' '{"kind":"TranslationUnitDecl","inner":[]}' ;;
	nonzero) printf '%s' '{"kind":"TranslationUnitDecl","inner":[]}'; exit 19 ;;
	extra-document)
		"$WOVE_CLANG_FIXTURE_CLANG" "$@"
		printf '\n{}\n'
		;;
	*) exit 1 ;;
esac

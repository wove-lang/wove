#!/bin/sh
# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

set -eu

python=$1
build=$2

# Check the process result even when the Wove program returns a nonzero value.
check_status() {
    expected=$1
    program=$2
    status=0
    "$program" || status=$?
    if [ "$status" -ne "$expected" ]; then
        printf '%s returned %s, expected %s\n' "$program" "$status" "$expected" >&2
        exit 1
    fi
}

# A source rejection must give the expected line and emit no C.
check_rejected() {
    fixture="tests/stage-1/$1.wove"
    line=$2
    status=0
    "$python" stage-1/wovec.py "$fixture" > "$build/rejected.c" 2> "$build/rejected.stderr" || status=$?
    if [ "$status" -ne 1 ] || [ -s "$build/rejected.c" ]; then
        printf '%s did not reject cleanly\n' "$fixture" >&2
        exit 1
    fi
    printf '%s:%s\n' "$fixture" "$line" > "$build/expected.stderr"
    diff -u "$build/expected.stderr" "$build/rejected.stderr"
}

check_status 6 "$build/ownership.transfer"
check_status 6 "$build/ownership.transfer.tracked"
check_status 3 "$build/ownership.mutable_borrow"
check_rejected ownership.use_after_move 19
check_rejected ownership.immutable_borrow 12

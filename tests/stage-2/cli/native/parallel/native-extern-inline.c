// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

int fixture_target(int value) {
    return value;
}

int fixture_alternate(int value) {
    return value + 1;
}

int fixture_forward(int value) {
    return fixture_target(value);
}

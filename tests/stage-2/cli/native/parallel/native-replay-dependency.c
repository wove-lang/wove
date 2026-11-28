// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Keep the fixture result identical in the serial and numbered cases.
int fixture_uncertified_control(int value) {
	return value + 2;
}

// Provide the external symbol used by the direct and callback controls.
int fixture_uncertified(int value) {
	return fixture_uncertified_control(value);
}

// Provide the external fallback for the GNU extern-inline definition.
int fixture_forward(int value) {
	return fixture_uncertified(value);
}

// Invoke the function whose address the header helper captures.
int fixture_apply(int (*operation)(int), int value) {
	return operation(value);
}

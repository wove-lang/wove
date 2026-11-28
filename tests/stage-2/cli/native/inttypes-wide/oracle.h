// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stddef.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>

static const wchar_t *const signed_inputs[] = {
    L" -?", L" -0x2a!", L"-42", L"-92233720368547758090!", L"0b101!"
};
static const wchar_t *const unsigned_inputs[] = {
    L" -?", L" +42!", L"52", L"184467440737095516160!", L"0b101!"
};

// Compare the exact native result, errno, and element offset under the active C profile.
static inline bool fixture_signed(int32_t test, int32_t base, intmax_t result,
    int32_t position, int32_t error, bool omitted) {
    const wchar_t *input = signed_inputs[test];
    wchar_t *end = NULL;
    errno = EDOM;
    intmax_t expected = wcstoimax(input, omitted ? NULL : &end, base);
    return result == expected && error == errno
        && (omitted ? position == -1 : position == end - input);
}

// Use the unsigned native prototype independently for range and prefix behavior.
static inline bool fixture_unsigned(int32_t test, int32_t base, uintmax_t result,
    int32_t position, int32_t error, bool omitted) {
    const wchar_t *input = unsigned_inputs[test];
    wchar_t *end = NULL;
    errno = EDOM;
    uintmax_t expected = wcstoumax(input, omitted ? NULL : &end, base);
    return result == expected && error == errno
        && (omitted ? position == -1 : position == end - input);
}

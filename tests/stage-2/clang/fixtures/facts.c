// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

_Static_assert(wvpr_header_present == WOVE_EXPECT_PRESENT, "header presence");
_Static_assert(wvpr_short_bits == sizeof(short) * 8, "short ABI");
_Static_assert(wvpr_int_bits == sizeof(int) * 8, "int ABI");
_Static_assert(wvpr_long_bits == sizeof(long) * 8, "long ABI");
_Static_assert(wvpr_long_long_bits == sizeof(long long) * 8, "long long ABI");
_Static_assert(wvpr_char_signed == ((char)-1 < 0), "char sign");
_Static_assert(wvpr_long_double_mantissa == __LDBL_MANT_DIG__, "long double ABI");
#if WOVE_EXPECT_PRESENT
_Static_assert(sizeof(ProbeFixture) == sizeof(int), "included declaration");
#endif

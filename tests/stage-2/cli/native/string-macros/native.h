/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#if defined(FUNCTION_LIKE)
#define FIXTURE_PATH() "unexpected"
#elif defined(WRONG_VALUE)
#define FIXTURE_PATH ((const char *)"pointer")
#elif defined(ENABLE_STRINGS)
#define FIXTURE_PATH FIXTURE_TEXT
#define FIXTURE_TEXT ("A\xff" "Z\0ignored")
#endif
#define FIXTURE_EMPTY ""
#define FIXTURE_UNUSED ("unused")
#define FIXTURE_NUMBER 39

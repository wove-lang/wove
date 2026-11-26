// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_TEST_POINTER_TYPEDEF_LOCALE_H
#define WOVE_TEST_POINTER_TYPEDEF_LOCALE_H

#include <ctype.h>
#include <locale.h>
#include <stdint.h>

// Darwin publishes the extended locale declarations separately.
#if defined(__APPLE__)
#include <xlocale.h>
#endif

typedef int32_t ScalarAlias;
typedef struct WrongLocaleTag *WrongLocale;

ScalarAlias make_scalar(void);
void drop_scalar(ScalarAlias value);
int32_t check_scalar(ScalarAlias value);
void wrong_drop(void *value);

#endif

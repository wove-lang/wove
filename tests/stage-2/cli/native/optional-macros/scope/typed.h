/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef SCOPE_PREREQUISITE
#error "required.h must precede typed.h"
#endif

typedef int WoveMacroType;

#if defined(ENABLE_SCOPE) && __STDC_VERSION__ == 201710L
#define SCOPE_READ 0.5f
#endif

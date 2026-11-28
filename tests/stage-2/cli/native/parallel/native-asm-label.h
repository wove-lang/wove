// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_ASM_LABEL_H
#define WOVE_PARALLEL_NATIVE_ASM_LABEL_H

#if defined(__APPLE__)
int fixture_read(int value) __asm__("_fixture_linked_read");
#else
int fixture_read(int value) __asm__("fixture_linked_read");
#endif

#endif

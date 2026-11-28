// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <sys/resource.h>

#if defined(WOVE_TEST_PROC_USAGE) && defined(__APPLE__)
// Select the same native declaration under the project bridge's header key.
#include <libproc.h>
#endif

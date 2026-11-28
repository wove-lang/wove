/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FENV_GLIBC_H
#define WOVE_TEST_FENV_GLIBC_H

#include <fenv.h>

/* Call the header preset directly so the fixture can compare target results. */
static inline int native_fenv_nomask(int update)
{
	return update ? feupdateenv(FE_NOMASK_ENV) : fesetenv(FE_NOMASK_ENV);
}

#if defined(__aarch64__)
/* Read the target's exception-to-control bit offset. */
static inline int native_fenv_except_shift(void)
{
	return FE_EXCEPT_SHIFT;
}
#endif

#endif

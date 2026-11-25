// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#if defined(__i386__) && __SIZEOF_LONG__ == 4
int wove_query_exact(void);
#endif
int wove_query_exact_suffix(void);

// Enum tags have a separate namespace from the probe's enumerator.
enum wvpr_header_present { wove_query_unrelated = 0 };

// Expose a declaration that identifies the language mode of the standalone probe.
#if __STDC_VERSION__ == 201112L
int wove_query_c11(void);
#elif __STDC_VERSION__ == 201710L
int wove_query_c17(void);
#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define KEEP (42)

#if defined(WRONG_OPTIONAL_KIND)
#define OPTIONAL_SAMPLE(value) ((float)(value))
#elif defined(ENABLE_OPTIONAL_VALUE)
static int reads;

// Reading the live expression evaluates the native call once.
static inline float sample_value(void) {
	++reads;
	return 42.0f;
}

// Expose the count without changing it.
static inline int sample_reads(void) { return reads; }

#define OPTIONAL_SAMPLE_BASE (sample_value())
#define OPTIONAL_SAMPLE OPTIONAL_SAMPLE_BASE
#endif

// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_STATIC_INLINE_H
#define WOVE_PARALLEL_NATIVE_STATIC_INLINE_H

static inline unsigned short fixture_identity16(unsigned short value) {
    return value;
}

static inline unsigned fixture_identity32(unsigned value) {
    return value;
}

static inline unsigned long long fixture_identity64(unsigned long long value) {
    return value;
}

static inline unsigned short fixture_swap16(unsigned short value) {
    return (unsigned short)((value >> 8) | ((value & 255u) << 8));
}

#if FIXTURE_MODE == 2
extern unsigned fixture_observe_swap32(unsigned, unsigned (*)(unsigned));

static inline unsigned fixture_swap32(unsigned value) {
    return fixture_observe_swap32(value, fixture_identity32);
}
#else
static inline unsigned fixture_swap32(unsigned value) {
#if FIXTURE_MODE == 1
    static unsigned calls;
    if (++calls == 1 && value != 0x01020304u)
        return 0;
#endif
    return ((value & 0xffu) << 24) | ((value & 0xff00u) << 8)
        | ((value >> 8) & 0xff00u) | (value >> 24);
}
#endif

static inline unsigned long long fixture_swap64(unsigned long long value) {
    return ((value & 0xffull) << 56) | ((value & 0xff00ull) << 40)
        | ((value & 0xff0000ull) << 24) | ((value & 0xff000000ull) << 8)
        | ((value >> 8) & 0xff000000ull) | ((value >> 24) & 0xff0000ull)
        | ((value >> 40) & 0xff00ull) | (value >> 56);
}

#if FIXTURE_MODE == 3
typedef __typeof__(&fixture_identity32) fixture_address_type;
#endif

#endif

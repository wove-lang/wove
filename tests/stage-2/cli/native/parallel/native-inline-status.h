// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_INLINE_STATUS_H
#define WOVE_PARALLEL_NATIVE_INLINE_STATUS_H

struct fixture_stream {
    int flags;
};

extern struct fixture_stream *fixture_status;
extern int fixture_eof(struct fixture_stream *stream);
extern int fixture_error(struct fixture_stream *stream);
extern void fixture_flags(int flags);
extern void fixture_close(struct fixture_stream *stream);

#if !defined(WOVE_INLINE_STATUS_IMPLEMENTATION)
extern inline __attribute__((gnu_inline)) int fixture_eof(struct fixture_stream *stream) {
    return (((stream)->flags & 16) != 0);
}

extern inline __attribute__((gnu_inline)) int fixture_error(struct fixture_stream *stream) {
    return (((stream)->flags & 32) == 32);
}
#endif

#endif

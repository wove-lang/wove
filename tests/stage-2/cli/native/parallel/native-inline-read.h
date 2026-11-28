// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_INLINE_READ_H
#define WOVE_PARALLEL_NATIVE_INLINE_READ_H

struct fixture_stream {
    char *next;
    char *end;
};

extern struct fixture_stream *fixture_input;
extern int fixture_refill(struct fixture_stream *stream);
extern int fixture_read(struct fixture_stream *stream);
extern void fixture_close(struct fixture_stream *stream);

#if !defined(WOVE_INLINE_READ_IMPLEMENTATION)
extern inline __attribute__((gnu_inline)) int fixture_read(struct fixture_stream *stream) {
    return (__builtin_expect((stream->next >= stream->end), 0)
        ? fixture_refill(stream) : *(unsigned char *)stream->next++);
}
#endif

#endif

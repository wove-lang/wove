// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_PARALLEL_NATIVE_INLINE_WRITE_H
#define WOVE_PARALLEL_NATIVE_INLINE_WRITE_H

struct fixture_writer {
    char *next;
    char *end;
};

extern struct fixture_writer *fixture_output;
extern int fixture_flush(struct fixture_writer *stream, int value);
extern int fixture_write(struct fixture_writer *stream, int value);
extern int fixture_written(int index);
extern void fixture_close(struct fixture_writer *stream);

#if !defined(WOVE_INLINE_WRITE_IMPLEMENTATION)
extern inline __attribute__((gnu_inline)) int fixture_write(struct fixture_writer *stream, int value) {
    return (__builtin_expect((stream->next >= stream->end), 0)
        ? fixture_flush(stream, (unsigned char)value) : (unsigned char)(*stream->next++ = value));
}
#endif

#endif

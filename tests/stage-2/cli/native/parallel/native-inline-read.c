// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define WOVE_INLINE_READ_IMPLEMENTATION
#include "native-inline-read.h"

static char fixture_bytes[] = { (char)225, 'Z' };
static struct fixture_stream fixture_buffer = { fixture_bytes, fixture_bytes };
struct fixture_stream *fixture_input = &fixture_buffer;

int fixture_refill(struct fixture_stream *stream) {
    if (stream->end != fixture_bytes)
        return -1;
    stream->next = fixture_bytes + 1;
    stream->end = fixture_bytes + sizeof fixture_bytes;
    return (unsigned char)fixture_bytes[0];
}

int fixture_read(struct fixture_stream *stream) {
    return stream->next >= stream->end ? fixture_refill(stream) : *(unsigned char *)stream->next++;
}

void fixture_close(struct fixture_stream *stream) {
    (void)stream;
}

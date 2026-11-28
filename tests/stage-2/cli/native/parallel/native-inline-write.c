// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define WOVE_INLINE_WRITE_IMPLEMENTATION
#include "native-inline-write.h"

static char buffer[2];
static unsigned char published[3];
static int length;
static struct fixture_writer output = { buffer, buffer + sizeof buffer };
struct fixture_writer *fixture_output = &output;

int fixture_flush(struct fixture_writer *stream, int value) {
    if (length != 0 || stream->next != stream->end)
        return -1;
    published[0] = (unsigned char)buffer[0];
    published[1] = (unsigned char)buffer[1];
    published[2] = (unsigned char)value;
    length = 3;
    return value;
}

int fixture_write(struct fixture_writer *stream, int value) {
    return stream->next >= stream->end
        ? fixture_flush(stream, (unsigned char)value) : (unsigned char)(*stream->next++ = value);
}

int fixture_written(int index) {
    return index >= 0 && index < length ? published[index] : -1;
}

void fixture_close(struct fixture_writer *stream) {
    (void)stream;
}

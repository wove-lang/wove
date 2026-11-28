// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#define WOVE_INLINE_STATUS_IMPLEMENTATION
#include "native-inline-status.h"

static struct fixture_stream status;
struct fixture_stream *fixture_status = &status;

int fixture_eof(struct fixture_stream *stream) {
    return (stream->flags & 16) != 0;
}

int fixture_error(struct fixture_stream *stream) {
    return (stream->flags & 32) == 32;
}

void fixture_flags(int flags) {
    status.flags = flags;
}

void fixture_close(struct fixture_stream *stream) {
    (void)stream;
}

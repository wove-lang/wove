// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#ifndef WOVE_FIXTURE_STREAM_VIEWS_H
#define WOVE_FIXTURE_STREAM_VIEWS_H

#if defined(__APPLE__)
typedef struct __sFILE FILE;
extern FILE *__stdoutp;
extern FILE *__stderrp;
#define stdout __stdoutp
#define stderr __stderrp
#else
typedef struct _IO_FILE FILE;
extern FILE *stdout;
extern FILE *stderr;
#define stdout stdout
#define stderr stderr
#endif

#if defined(STREAM_LOCAL)
static int local_state __attribute__((used));
#endif

extern int fputc(int, FILE *);

#endif

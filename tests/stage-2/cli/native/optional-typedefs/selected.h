/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_OPTIONAL_TYPEDEF_H
#define WOVE_TEST_OPTIONAL_TYPEDEF_H

#if defined(SUPPLY_TYPEDEF)
typedef unsigned int native_base_t;
#if defined(TYPEOF_EXPR)
typedef __typeof__(0U) native_alias_t;
#elif defined(TYPEOF_TYPE)
typedef __typeof__(unsigned int) native_alias_t;
#else
typedef native_base_t native_alias_t;
#endif
typedef enum { TEST_MODE = 22 } native_mode_t;
typedef struct { int adjustment; } native_packet_t;
#elif defined(WRONG_KIND)
int native_alias_t(void);
#elif defined(WRONG_RECORD)
typedef struct { unsigned int value; } native_alias_t;
#elif defined(WRONG_POINTER)
typedef unsigned int *native_alias_t;
#elif defined(WRONG_VOID)
typedef void native_alias_t;
#elif defined(TYPEOF_RECORD)
struct native_typeof_record { unsigned int field; };
extern struct native_typeof_record native_typeof_value;
typedef __typeof__(native_typeof_value) native_alias_t;
#elif defined(TYPEOF_POINTER)
typedef __typeof__((unsigned int *)0) native_alias_t;
#else
typedef unsigned int __native_alias_t;
#endif

#endif

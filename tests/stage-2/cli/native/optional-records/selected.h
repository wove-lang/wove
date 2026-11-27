/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#if defined(SUPPLY_RECORDS)
struct packet;
typedef struct packet packet_before_definition;
struct packet;
struct packet { int value; };
struct packet;
union variant { int value; unsigned long other; };
typedef struct { int value; } anonymous_packet;
typedef anonymous_packet packet_alias;
struct outer {
	struct nested_record { int value; } item;
	union nested_variant { int value; unsigned long other; } choice;
};
#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#if defined(FORWARD_ONLY)
struct packet;
typedef struct packet packet_alias;
#elif defined(BLOCK_RECORD)
struct packet;

/* This record is scoped to the function that declares it. */
static inline int fixture_local(void)
{
	struct packet { int value; };
	return sizeof(struct packet) == sizeof(int);
}
#elif defined(WRONG_TAG)
union packet { int value; };
#elif defined(POINTER_ALIAS)
struct packet { int value; };
typedef struct packet *packet_alias;
#endif

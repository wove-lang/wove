/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_GRP_H
#define WOVE_TEST_GRP_H

#include <grp.h>
#include <stddef.h>
#include <stdint.h>

_Static_assert(sizeof(struct group) == 32 && _Alignof(struct group) == 8, "native group layout");
_Static_assert(offsetof(struct group, gr_name) == 0, "native group name");
_Static_assert(offsetof(struct group, gr_passwd) == 8, "native group password");
_Static_assert(offsetof(struct group, gr_gid) == 16, "native group ID");
_Static_assert(offsetof(struct group, gr_mem) == 24, "native group members");
_Static_assert(_Generic(((struct group *)0)->gr_gid, uint32_t: 1, default: 0), "unsigned group ID");

/* Read the SDK record populated by Wove and admit the native size_t argument. */
static inline uint32_t fixture_group_id(const struct group *record, size_t width)
{
	return width == sizeof(size_t) ? record->gr_gid : 0;
}

#if defined(__APPLE__) || defined(__USE_XOPEN) || defined(__USE_MISC)
/* Check the exact reset and close signatures and read one native entry. */
static inline int fixture_group_warm(void)
{
	void (*reset)(void) = setgrent;
	void (*close)(void) = endgrent;
	(void)reset;
	(void)close;
	return getgrent() != NULL;
}
#endif

#if defined(__APPLE__) && defined(_DARWIN_C_SOURCE)
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Choose an absent ID in a range that is outside ordinary allocation. */
static inline gid_t fixture_group_missing(void)
{
	/* Keep the lookup bounded even with a custom group directory. */
	for (gid_t id = UINT32_C(4000000000); id < UINT32_C(4000000032); ++id) {
		if (getgrgid(id) == NULL)
			return id;
	}
	return (gid_t)-1;
}

/* Check the SDK prototypes and compare copied bytes to an independent lookup. */
static inline int fixture_group_matches(gid_t id, const char *name)
{
	char *(*by_id)(gid_t, int) = group_from_gid;
	int (*rewind)(int) = setgroupent;
	(void)by_id;
	(void)rewind;
	struct group *record = getgrgid(id);
	if (record != NULL)
		return strcmp(record->gr_name, name) == 0;
	char decimal[16];
	return snprintf(decimal, sizeof decimal, "%u", (unsigned)id) > 0 && strcmp(decimal, name) == 0;
}
#endif

#if defined(__GLIBC__) && defined(__USE_MISC)
#include <errno.h>
#include <unistd.h>

/* Compare the imported macro with the selected SDK's definition. */
static inline int fixture_group_buffer_size(void)
{
	return NSS_BUFLEN_GROUP;
}

/* Check exact prototypes and accept hosts that withhold CAP_SETGID. */
static inline int fixture_group_status(int status)
{
	int (*initialize)(const char *, gid_t) = initgroups;
	int (*replace)(size_t, const gid_t *) = setgroups;
	(void)initialize;
	(void)replace;
	return status == 0 || (status == -1 && errno == EPERM);
}

/* Read back the single group set in the fixture process when permitted. */
static inline int fixture_group_selected(gid_t id)
{
	gid_t groups[1];
	return getgroups(1, groups) == 1 && groups[0] == id;
}
#endif

#endif

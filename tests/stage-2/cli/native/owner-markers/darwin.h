/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_DARWIN_OWNER_MARKER_H
#define WOVE_TEST_DARWIN_OWNER_MARKER_H

#include <stddef.h>
#include <sys/acl.h>
#include <uuid/uuid.h>

/* Create one real extended ACL entry that survives the filesec copy. */
static inline acl_t make_acl_fixture(void)
{
	acl_t acl = acl_init(1);
	if (acl == NULL) {
		return NULL;
	}
	acl_entry_t entry;
	acl_permset_t permissions;
	uuid_t qualifier = {0x37, 0x55, 0x61, 0x87};
	if (acl_create_entry(&acl, &entry) != 0 ||
	    acl_set_tag_type(entry, ACL_EXTENDED_ALLOW) != 0 ||
	    acl_set_qualifier(entry, qualifier) != 0 ||
	    acl_get_permset(entry, &permissions) != 0 ||
	    acl_add_perm(permissions, ACL_READ_DATA) != 0) {
		(void)acl_free(acl);
		return NULL;
	}
	return acl;
}

/* Verify the returned owned ACL is valid and exposes the expected entry. */
static inline int check_acl_fixture(acl_t acl)
{
	acl_entry_t entry;
	acl_tag_t tag;
	acl_permset_t permissions;
	return acl_valid(acl) == 0 &&
		acl_get_entry(acl, ACL_FIRST_ENTRY, &entry) == 0 &&
		acl_get_tag_type(entry, &tag) == 0 && tag == ACL_EXTENDED_ALLOW &&
		acl_get_permset(entry, &permissions) == 0 &&
		acl_get_perm_np(permissions, ACL_READ_DATA) == 1 ? 0 : -1;
}

#endif

/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_FILESEC_ACL_H
#define WOVE_TEST_FILESEC_ACL_H

#include "./acl-fixture.h"
#include <errno.h>
#include <fcntl.h>

/* Set an ACL with the native API independently of the Wove setter. */
static inline int seed_filesec_acl(filesec_t security, acl_t acl)
{
	return filesec_set_property(security, FILESEC_ACL, &acl);
}

/* Set the remove marker independently of the Wove removal action. */
static inline int seed_filesec_marker(filesec_t security)
{
	return filesec_set_property(security, FILESEC_ACL, _FILESEC_REMOVE_ACL);
}

/* Inspect the raw property and release only a real owned ACL returned by libc. */
static inline int native_acl_state(filesec_t security)
{
	acl_t output = NULL;
	int status = filesec_get_property(security, FILESEC_ACL, &output);
	if (status != 0) {
		return status == -1 && errno == ENOENT && output == NULL ? 0 : -1;
	}
	if (output == _FILESEC_REMOVE_ACL) {
		return 2;
	}
	if (output == NULL) {
		return -1;
	}
	int result = check_acl_fixture(output) == 0 ? 1 : -1;
	(void)acl_free(output);
	return result;
}

#endif

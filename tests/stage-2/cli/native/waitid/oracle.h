/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#ifndef WOVE_TEST_WAITID_H
#define WOVE_TEST_WAITID_H

#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

/* Check the exact native prototype and the SDK's three portable selectors. */
static inline int fixture_wait_selectors(idtype_t all, idtype_t pid, idtype_t pgid)
{
	int (*native)(idtype_t, id_t, siginfo_t *, int) = waitid;
	(void)native;
	return all == P_ALL && pid == P_PID && pgid == P_PGID;
}

/* Compare all published wait options to the selected native header. */
static inline int fixture_wait_flags(int continued, int exited, int nohang,
				     int nowait, int stopped, int untraced)
{
	return continued == WCONTINUED && exited == WEXITED && nohang == WNOHANG
	       && nowait == WNOWAIT && stopped == WSTOPPED && untraced == WUNTRACED;
}

/* A normally exited child reports the target's SIGCHLD and CLD_EXITED. */
static inline int fixture_wait_kind(int signal, int code)
{
	return signal == SIGCHLD && code == CLD_EXITED;
}

/* glibc reports the child's real UID. Darwin may leave the zero value. */
static inline int fixture_wait_user(uint32_t uid)
{
#if defined(__APPLE__)
	return uid == (uint32_t)getuid() || uid == 0;
#else
	return uid == (uint32_t)getuid();
#endif
}

#endif

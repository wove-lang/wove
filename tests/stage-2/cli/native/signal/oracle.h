// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>

_Static_assert(_Generic((sig_atomic_t)0, int: 1, default: 0), "native sig_atomic_t type");
_Static_assert(_Generic(&raise, int (*)(int): 1, default: 0), "native raise prototype");

static volatile sig_atomic_t observed_signal;

// Record one signal delivered to this fixture process.
static void fixture_record_signal(int signum)
{
	observed_signal = signum;
}

// Install a handler in the process that will call raise.
static inline bool fixture_catch(int32_t signum)
{
	observed_signal = 0;
	return signal(signum, fixture_record_signal) != SIG_ERR;
}

// Read the signal through the exact native scalar type.
static inline sig_atomic_t fixture_seen(void)
{
	return observed_signal;
}

// Compare each selected constant with the native header under the same flags.
static inline bool fixture_signals(
	int32_t sigabrt, int32_t sigfpe, int32_t sigill, int32_t sigint,
	int32_t sigsegv, int32_t sigterm, int32_t sighup, int32_t sigquit,
	int32_t sigtrap, int32_t sigbus, int32_t sigkill, int32_t sigusr1,
	int32_t sigusr2, int32_t sigpipe, int32_t sigalrm, int32_t sigchld,
	int32_t sigcont, int32_t sigstop, int32_t sigtstp, int32_t sigttin,
	int32_t sigttou, int32_t sigurg, int32_t sigxcpu, int32_t sigxfsz,
	int32_t sigvtalrm, int32_t sigprof)
{
	return
		sigabrt == SIGABRT && sigfpe == SIGFPE && sigill == SIGILL &&
		sigint == SIGINT && sigsegv == SIGSEGV && sigterm == SIGTERM &&
		sighup == SIGHUP && sigquit == SIGQUIT && sigtrap == SIGTRAP &&
		sigbus == SIGBUS && sigkill == SIGKILL && sigusr1 == SIGUSR1 &&
		sigusr2 == SIGUSR2 && sigpipe == SIGPIPE && sigalrm == SIGALRM &&
		sigchld == SIGCHLD && sigcont == SIGCONT && sigstop == SIGSTOP &&
		sigtstp == SIGTSTP && sigttin == SIGTTIN && sigttou == SIGTTOU &&
		sigurg == SIGURG && sigxcpu == SIGXCPU && sigxfsz == SIGXFSZ &&
		sigvtalrm == SIGVTALRM && sigprof == SIGPROF;
}

#ifdef TEST_MEMBER
// Read the selected macro directly in C for the optional member case.
static inline int32_t fixture_signal_member(void)
{
	return TEST_MEMBER;
}
#endif

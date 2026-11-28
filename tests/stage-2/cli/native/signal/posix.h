// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <signal.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

_Static_assert(_Generic(&kill, int (*)(pid_t, int): 1, default: 0), "native kill prototype");
_Static_assert(_Generic(&killpg, int (*)(pid_t, int): 1, default: 0), "native killpg prototype");

static volatile sig_atomic_t observed_signal;

// Record delivery to this fixture process.
static void fixture_record_signal(int signum)
{
	observed_signal = signum;
}

// Confirm the process owns its group before handing either ID to Wove.
static inline pid_t fixture_isolate(int32_t signum)
{
	pid_t pid = getpid();
	// The test spawns no children. A positive group ID equal to the PID is private to it.
	if (pid <= 0 || (getpgrp() != pid && setpgid(0, 0) != 0) || getpgrp() != pid) {
		return (pid_t)-1;
	}

	struct sigaction action = {0};
	action.sa_handler = fixture_record_signal;
	sigset_t unblock;
	observed_signal = 0;
	// Install the handler before unblocking delivery and passing the verified ID back.
	if (sigemptyset(&action.sa_mask) != 0 || sigemptyset(&unblock) != 0
		|| sigaddset(&unblock, signum) != 0 || sigaction(signum, &action, NULL) != 0
		|| sigprocmask(SIG_UNBLOCK, &unblock, NULL) != 0) {
		return (pid_t)-1;
	}
	return pid;
}

// Check the signal recorded by the handler after the native call.
static inline bool fixture_received(int32_t signum)
{
	return observed_signal == signum;
}

# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import fcntl
import json
import os
from pathlib import Path
import sys
import time

ledger = Path("jobs.json")
payload = 2 * 1024 * 1024


# Serialize observations across independent child processes.
def record(change):
    with ledger.open("a+", encoding="utf-8") as stream:
        fcntl.flock(stream, fcntl.LOCK_EX)
        stream.seek(0)
        data = stream.read()
        state = json.loads(data) if data else {"started": [], "active": [], "peak": 0}
        change(state)
        stream.seek(0)
        stream.truncate()
        json.dump(state, stream)
        return state


# Bound fixture rendezvous so a broken scheduler fails instead of hanging the test.
def await_path(name):
    path = Path(name)
    end = time.monotonic() + 10
    while not path.exists():
        if time.monotonic() >= end:
            raise RuntimeError(f"timed out waiting for {name}")
        time.sleep(0.005)


# Keep the sibling alive until the runner has actually reaped the failing PID.
def await_reap(name):
    await_path(name)
    pid = int(Path(name).read_text())
    end = time.monotonic() + 10
    # A completed child keeps its PID until wait has collected it.
    while time.monotonic() < end:
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return
        time.sleep(0.005)
    raise RuntimeError(f"timed out waiting for the runner to reap {pid}")


# Prove overlap and refill with a barrier that depends on a later job.
def execute(mode, job):
    def enter(state):
        state["started"].append(job)
        state["active"].append(job)
        state["peak"] = max(state["peak"], len(state["active"]))

    record(enter)
    Path(f"started-{job}").touch()
    try:
        if mode == "parallel" and job == 0:
            await_path("started-2")
        elif mode == "parallel" and job == 1:
            await_path("started-0")
        elif mode in {"failure", "order"} and job == 0:
            await_reap("failing")
        elif mode in {"failure", "order"} and job == 1:
            await_path("started-0")
            marker = Path("failing.pending")
            marker.write_text(str(os.getpid()))
            marker.rename("failing")
            return 22
        sys.stdout.buffer.write(f"stdout-{job}:".encode() + b"x" * payload)
        sys.stderr.buffer.write(f"stderr-{job}:".encode() + b"y" * payload)
        return 7 if mode == "order" and job == 0 else 0
    finally:
        record(lambda state: state["active"].remove(job))


# Check exactly which jobs ran, their high-water mark, and their redirected bytes.
def audit(mode):
    state = record(lambda _: None)
    expected = {"serial": [0, 1, 2, 3], "parallel": [0, 1, 2, 3],
                "failure": [0, 1], "order": [0, 1], "launch": [0],
                "wide-budget": [0, 1, 2, 3]}[mode]
    assert sorted(state["started"]) == expected and not state["active"], state
    if mode == "serial":
        assert state["started"] == expected and state["peak"] == 1, state
    elif mode in {"parallel", "failure", "order"}:
        assert state["peak"] == 2, state
    elif mode == "launch":
        assert state["peak"] == 1, state
    for job in expected:
        for stream, byte in (("stdout", b"x"), ("stderr", b"y")):
            data = Path(f"{stream}-{job}").read_bytes()
            wanted = b"" if mode in {"failure", "order"} and job == 1 else (
                f"{stream}-{job}:".encode() + byte * payload
            )
            assert data == wanted, (job, stream, len(data))
    return 0


# A separately launched process stays waitable by its original owner.
def main():
    mode, tag = sys.argv[1:]
    if tag == "u":
        return 37
    if tag == "a":
        return audit(mode)
    return execute(mode, int(tag))


if __name__ == "__main__":
    sys.exit(main())

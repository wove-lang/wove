# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

import fcntl
import json
import os
from pathlib import Path
import sys
import time

ledger = Path("jobs.json")


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


# Wait until the runner has collected a child whose exit marker is present.
def await_reap(name):
    await_path(name)
    pid = int(Path(name).read_text())
    end = time.monotonic() + 10
    while time.monotonic() < end:
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return
        time.sleep(0.005)
    raise RuntimeError(f"timed out waiting for the runner to reap {pid}")


# Publish a complete marker at once so readers never see a partial PID.
def mark(phase, job):
    marker = Path(f"{phase}-{job}")
    pending = marker.with_suffix(".pending")
    pending.write_text(str(os.getpid()))
    pending.rename(marker)


# Build the configured bytes independently of the file redirection.
def payload(spec, job):
    return spec["prefix"].format(job=job).encode() + spec["fill"].encode() * spec["length"]


# Run a fixture's specified rendezvous, writes and exit status.
def execute(scenario, streams, job):
    def enter(state):
        state["started"].append(job)
        state["active"].append(job)
        state["peak"] = max(state["peak"], len(state["active"]))

    record(enter)
    mark("started", job)
    planned = scenario.get("jobs", {}).get(str(job), {})
    try:
        for event, other_job in planned.get("wait", []):
            if event == "started":
                await_path(f"started-{other_job}")
            elif event == "reaped":
                await_reap(f"exiting-{other_job}")
            else:
                raise ValueError(f"unsupported rendezvous event {event}")
        for name, spec in streams.items():
            getattr(sys, name).buffer.write(payload(spec, job))
        return planned.get("status", 0)
    finally:
        record(lambda state: state["active"].remove(job))
        mark("exiting", job)


# Check the recorded scheduler observations and the bytes in the supplied log files.
def audit(expected, streams):
    state = record(lambda _: None)
    started = state["started"] if expected.get("ordered") else sorted(state["started"])
    assert started == expected["started"] and not state["active"], state
    if "peak" in expected:
        assert state["peak"] == expected["peak"], state
    for job in started:
        for name, spec in streams.items():
            data = Path(spec["filename"].format(job=job)).read_bytes()
            assert data == payload(spec, job), (job, name, len(data))
    return 0


# Dispatch a worker, a separate owned child, or a post-run audit.
def main():
    manifest, case, tag = sys.argv[1:]
    data = json.loads("\n".join(line for line in manifest.splitlines() if not line.startswith("//")))
    if tag == "u":
        return data["unrelated_status"]
    scenario = data["cases"][case]
    if tag == "a":
        return audit(scenario["expected"], data["streams"])
    return execute(scenario, data["streams"], int(tag))


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/python3
#
# Copyright 2026 Centreon
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# For more information : contact@centreon.com
#
"""Kernel counters a benchmark needs, read straight from /proc.

Every CPU figure produced here comes from a *cumulative* counter, never from an
instantaneous sample: utime+stime in /proc/PID/stat counts every tick the kernel
charged to the process, so the difference between two reads divided by the
elapsed time is the exact average over the whole window.  Sampling would miss
what happens between two samples, and the CPU of centengine is spiky -- checks
leave in waves at the interval boundaries, so a one-second probe every minute
catches a random phase of that wave.

Stdlib only, on purpose: this module is also what a copy of the probe dropped on
a customer installation would rely on.
"""

import os
import subprocess
import time
from typing import NamedTuple, Optional

CLK_TCK = os.sysconf("SC_CLK_TCK")


class ProcStat(NamedTuple):
    """One read of /proc/PID/stat, reduced to what a benchmark uses."""

    pid: int
    comm: str
    # Field 22, the process start time in ticks since boot. Read alongside the
    # counters so that a daemon restarting mid-window can be detected: its
    # counters would go back to zero and the delta would be nonsense.
    starttime: int
    # utime + stime. Children are excluded on purpose: centengine forks a plugin
    # per check and their CPU is not its own.
    cpu_ticks: int


class ProcMem(NamedTuple):
    """Memory footprint of a process, in kilobytes, plus its thread count."""

    rss_kb: int
    swap_kb: int
    threads: int


class CommandCost(NamedTuple):
    """What running a command to completion cost, from the kernel's own rusage."""

    wall_s: float
    user_s: float
    sys_s: float
    # ru_maxrss, the high water mark of the resident set. Linux reports it in
    # kilobytes already, no conversion needed.
    maxrss_kb: int
    exit_code: int
    output: str


def read_proc_stat(pid: int) -> Optional[ProcStat]:
    """Read the cumulative CPU counters of a process.

    Args:
        pid (int): the process to read.

    Returns:
        A ProcStat, or None if the process is gone.
    """
    try:
        with open(f"/proc/{pid}/stat", "rb") as f:
            raw = f.read().decode("utf-8", "replace")
    except OSError:
        return None

    # The comm field is parenthesised and may itself contain spaces and
    # parentheses, so everything before the last ") " is skipped and the comm
    # taken from between the first "(" and that same delimiter.
    cut = raw.rfind(") ")
    if cut < 0:
        return None
    comm = raw[raw.find("(") + 1:cut]
    fields = raw[cut + 2:].split()
    try:
        # fields[0] is state, so field N of proc(5) is fields[N - 3].
        return ProcStat(pid=pid, comm=comm,
                        starttime=int(fields[19]),
                        cpu_ticks=int(fields[11]) + int(fields[12]))
    except (IndexError, ValueError):
        return None


def read_proc_mem(pid: int) -> Optional[ProcMem]:
    """Read the memory footprint of a process.

    Args:
        pid (int): the process to read.

    Returns:
        A ProcMem, or None if the process is gone.
    """
    rss = swap = threads = 0
    try:
        with open(f"/proc/{pid}/status", "r") as f:
            for line in f:
                key, _, value = line.partition(":")
                if key == "VmRSS":
                    rss = int(value.split()[0])
                elif key == "VmSwap":
                    swap = int(value.split()[0])
                elif key == "Threads":
                    threads = int(value.split()[0])
    except (OSError, IndexError, ValueError):
        return None
    return ProcMem(rss_kb=rss, swap_kb=swap, threads=threads)


def read_proc_io(pid: int) -> Optional[tuple[int, int]]:
    """Read the I/O volume a process has caused.

    Args:
        pid (int): the process to read.

    Returns:
        (read_bytes, write_bytes) as accounted by the block layer, or None when
        the file cannot be read -- /proc/PID/io is owner-or-root only, so an
        unprivileged benchmark simply goes without.
    """
    read_bytes = write_bytes = 0
    try:
        with open(f"/proc/{pid}/io", "r") as f:
            for line in f:
                key, _, value = line.partition(":")
                if key == "read_bytes":
                    read_bytes = int(value)
                elif key == "write_bytes":
                    write_bytes = int(value)
    except (OSError, ValueError):
        return None
    return read_bytes, write_bytes


def read_machine() -> tuple[int, int]:
    """Read the machine-wide CPU counters.

    Returns:
        (busy, total) in jiffies. "Busy" leaves out idle and iowait: a machine
        waiting on its disks is not a machine burning CPU.
    """
    with open("/proc/stat", "r") as f:
        fields = f.readline().split()
    values = [int(v) for v in fields[1:]]
    total = sum(values)
    # values[3] is idle, values[4] iowait.
    return total - values[3] - values[4], total


def find_pids(comm: str, cmdline_match: Optional[str] = None) -> list[int]:
    """Find the running processes matching a command name.

    /proc is scanned rather than pgrep being called, because our own command
    line carries the very strings we are looking for and pgrep would match the
    benchmark itself.

    Args:
        comm (str): exact content of /proc/PID/comm, e.g. "cbd".
        cmdline_match (str, optional): substring the command line must contain,
            to tell two daemons sharing a binary apart. Defaults to None.

    Returns:
        The list of matching pids, ours excluded.
    """
    own = os.getpid()
    found = []
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        pid = int(entry)
        if pid == own:
            continue
        try:
            with open(f"/proc/{pid}/comm", "r") as f:
                if f.read().strip() != comm:
                    continue
            if cmdline_match is not None:
                with open(f"/proc/{pid}/cmdline", "rb") as f:
                    cmdline = f.read().replace(b"\0", b" ").decode(
                        "utf-8", "replace")
                if cmdline_match not in cmdline:
                    continue
        except OSError:
            continue
        found.append(pid)
    return found


def cpu_seconds(ticks: int) -> float:
    """Convert a tick count into seconds.

    Args:
        ticks (int): a delta of utime+stime.

    Returns:
        The equivalent number of CPU seconds.
    """
    return ticks / CLK_TCK


class ProcessProbe:
    """Cumulative CPU and memory of one process, over a window.

    The probe holds the counters read when it was opened; every figure it
    returns afterwards is a delta against those, so nothing that happens between
    two samples is lost.
    """

    def __init__(self, name: str, pid: int):
        """Open a probe on a running process.

        Args:
            name (str): how the process is named in the results, e.g.
                "central-broker".
            pid (int): the process to follow.

        Raises:
            RuntimeError: if the process cannot be read at all.
        """
        self.name = name
        self.pid = pid
        stat = read_proc_stat(pid)
        if stat is None:
            raise RuntimeError(f"{name}: cannot read /proc/{pid}/stat")
        self._start = stat
        self._prev_ticks = stat.cpu_ticks
        mem = read_proc_mem(pid)
        self.rss_first_kb = mem.rss_kb if mem else 0
        self.rss_max_kb = self.rss_first_kb
        self.rss_last_kb = self.rss_first_kb
        self.swap_last_kb = mem.swap_kb if mem else 0
        self.threads_last = mem.threads if mem else 0

    def restarted(self) -> bool:
        """Tell whether the process was restarted since the probe was opened.

        Returns:
            True when the pid is gone or now carries a different start time, in
            which case its counters went back to zero and the whole window is
            void.
        """
        stat = read_proc_stat(self.pid)
        return stat is None or stat.starttime != self._start.starttime

    def sample(self, interval_s: float) -> Optional[dict]:
        """Take one sample, and fold it into the running maxima.

        Args:
            interval_s (float): seconds elapsed since the previous sample, used
                to turn the tick delta into a percentage.

        Returns:
            A dict of the sampled values, or None if the process is gone.
        """
        stat = read_proc_stat(self.pid)
        mem = read_proc_mem(self.pid)
        if stat is None or mem is None:
            return None
        delta = stat.cpu_ticks - self._prev_ticks
        self._prev_ticks = stat.cpu_ticks
        self.rss_last_kb = mem.rss_kb
        self.swap_last_kb = mem.swap_kb
        self.threads_last = mem.threads
        self.rss_max_kb = max(self.rss_max_kb, mem.rss_kb)
        return {
            "cpu_pct": cpu_seconds(delta) / interval_s * 100 if interval_s else 0.0,
            "rss_kb": float(mem.rss_kb),
            "swap_kb": float(mem.swap_kb),
            "threads": float(mem.threads),
        }

    def total_cpu_s(self) -> float:
        """Return the CPU seconds burnt since the probe was opened.

        Returns:
            utime+stime consumed over the window, in seconds, or 0 if the
            process vanished.
        """
        stat = read_proc_stat(self.pid)
        if stat is None:
            return 0.0
        return cpu_seconds(stat.cpu_ticks - self._start.cpu_ticks)


def measure_command(argv: list[str], cwd: Optional[str] = None,
                    env: Optional[dict] = None) -> CommandCost:
    """Run a command to completion and report what it cost.

    The figures come from wait4(), so they are the kernel's own accounting of
    that one child: no sampling, and a peak RSS that a poll would likely miss.

    Args:
        argv (list): the command and its arguments.
        cwd (str, optional): working directory. Defaults to None.
        env (dict, optional): environment. Defaults to None, inherit.

    Returns:
        A CommandCost. Its output holds stdout and stderr merged.
    """
    t0 = time.monotonic()
    proc = subprocess.Popen(argv, cwd=cwd, env=env,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    # Draining the pipe before waiting, otherwise a command writing more than a
    # pipeful -- centengine -v prints a report -- would block for ever.
    pipe = proc.stdout
    assert pipe is not None  # stdout=PIPE above, so it is always there
    output = pipe.read().decode("utf-8", "replace")
    pipe.close()
    _, status, rusage = os.wait4(proc.pid, 0)
    wall = time.monotonic() - t0
    # The child has just been reaped by hand, so Popen must be told, or its
    # destructor complains about a still-running subprocess and tries to wait
    # on a pid that no longer exists.
    proc.returncode = os.waitstatus_to_exitcode(status)
    return CommandCost(wall_s=wall,
                       user_s=rusage.ru_utime,
                       sys_s=rusage.ru_stime,
                       maxrss_kb=int(rusage.ru_maxrss),
                       exit_code=proc.returncode,
                       output=output)

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
"""Driving heaptrack from a benchmark: attach, detach, read the trace.

Attaching to a process already running is what the allocation benchmark needs --
centengine has to be warm, and its configuration applied, before the tracer comes
in. heaptrack does that with -p, and everything delicate about it is handled here.

What was learnt the hard way, and is why this module exists rather than a couple
of shell lines:

* ``-o`` must come before ``-p``. Once heaptrack 1.5 has parsed -p it stops
  reading options and silently writes to the current directory.
* the trace extension depends on the compressor the build was made with: .zst on
  some, .gz on the one in the test container. Never assume, glob.
* the injection can kill the debuggee. It goes through gdb calling a function in
  the target, and a target sitting in the wrong place -- an interpreter, a
  cancellation point -- can take a SIGSEGV from it. So the target is checked
  alive after the injection rather than assumed to be.
* there is no way to detach heaptrack 1.5.0 from a process and leave it running.
  Signalling the tracer kills the script without its ``trap cleanup EXIT`` --
  measured: SIGINT left the injection in place and the trace growing until the
  target died on its own -- and the cleanup itself, which calls
  ``heaptrack_stop()`` through gdb, aborts the debuggee on this build:
  ``Assertion '!s_forceCleanup' failed``. The manual Ctrl-C has the same effect.
  So the supported path is the only one used here: the caller stops the traced
  process, heaptrack flushes its trace and exits by itself, and finish() just
  waits for that. force_stop() exists for the case where the target cannot be
  stopped, and says out loud that it will kill it.
* a heaptrack started as a background job from a non-interactive shell inherits
  SIGINT *ignored*, so no signal can reach it at all in that setting. One more
  reason for the benchmark to drive it from here rather than from a shell script.
"""

import glob
import os
import signal
import subprocess
import time
from typing import NamedTuple, Optional


def _gdb_stop_argv(pid: int) -> list[str]:
    """Build the command that removes the heaptrack injection from a process.

    This is, argument for argument and in the same order, what the cleanup trap
    of /usr/bin/heaptrack runs. Calling it ourselves is what makes the detach
    deterministic: heaptrack_stop() closes the target's side of the FIFO, the
    reading pipeline then sees EOF, and the script leaves its wait and exits
    normally -- running its own cleanup on the way out.

    Args:
        pid (int): the traced process.

    Returns:
        The argv to run.
    """
    return ["gdb", "--batch-silent", "-n",
            "-iex=set auto-solib-add off", "-iex=set language c",
            "-p", str(pid),
            "--eval-command=sharedlibrary libheaptrack_inject",
            "--eval-command=call (void) heaptrack_stop()",
            "--eval-command=detach"]


class Tracer(NamedTuple):
    """A heaptrack attached to a running process."""

    proc: subprocess.Popen
    target_pid: int
    trace_path: str
    log_path: str


class HeaptrackError(RuntimeError):
    """Raised when heaptrack cannot be attached, or cannot be stopped."""


def available() -> bool:
    """Tell whether heaptrack and its reader are installed.

    Returns:
        True when both heaptrack and heaptrack_print can be run.
    """
    from shutil import which
    return bool(which("heaptrack")) and bool(which("heaptrack_print"))


def find_trace(output_prefix: str) -> Optional[str]:
    """Find the trace file heaptrack wrote for a given -o prefix.

    Args:
        output_prefix (str): what was passed to -o, without extension.

    Returns:
        The trace path, or None if heaptrack has not created it yet. The
        extension is whatever compressor the build uses, so it is globbed.
    """
    matches = [p for p in glob.glob(f"{output_prefix}.*")
               if not p.endswith(".log")]
    return matches[0] if matches else None


def _alive(pid: int) -> bool:
    """Tell whether a pid still exists.

    Args:
        pid (int): the process to test.

    Returns:
        True if the process is there.
    """
    return os.path.isdir(f"/proc/{pid}")


def attach(pid: int, output_prefix: str, timeout: float = 120.0) -> Tracer:
    """Attach heaptrack to a running process.

    Args:
        pid (int): the process to trace.
        output_prefix (str): -o value; the extension is added by heaptrack.
        timeout (float, optional): seconds to wait for the injection. Defaults
            to 120, because injecting through gdb into a large binary such as
            centengine takes tens of seconds.

    Returns:
        The Tracer, once the injection is finished and the target is still alive.

    Raises:
        HeaptrackError: if the injection fails, times out, or kills the target.
    """
    if not _alive(pid):
        raise HeaptrackError(f"process {pid} is not running")
    os.makedirs(os.path.dirname(os.path.abspath(output_prefix)),
                mode=0o775, exist_ok=True)
    log_path = f"{output_prefix}.log"
    log = open(log_path, "w")
    # start_new_session so that the tracer has its own process group: a signal
    # meant for it can then never reach the benchmark, nor the other way round.
    proc = subprocess.Popen(["heaptrack", "-o", output_prefix, "-p", str(pid)],
                            stdout=log, stderr=subprocess.STDOUT,
                            start_new_session=True)
    log.close()

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        time.sleep(1)
        if not _alive(pid):
            _terminate(proc, 10)
            raise HeaptrackError(
                f"the injection killed the target ({pid}): see {log_path}. "
                "It goes through gdb calling into the process, which some "
                "targets do not survive.")
        if proc.poll() is not None:
            raise HeaptrackError(
                f"heaptrack gave up (exit {proc.returncode}): see {log_path}")
        with open(log_path, "r") as f:
            text = f.read()
        if "injection finished" in text:
            trace = find_trace(output_prefix)
            if trace is None:
                raise HeaptrackError(
                    f"heaptrack says it is injected but wrote no trace next to "
                    f"{output_prefix}")
            return Tracer(proc=proc, target_pid=pid, trace_path=trace,
                          log_path=log_path)
    _terminate(proc, 10)
    raise HeaptrackError(f"heaptrack did not finish injecting within "
                         f"{timeout:.0f}s: see {log_path}")


def _terminate(proc: subprocess.Popen, timeout: float):
    """Make a process go away, politely then not.

    Args:
        proc: the process.
        timeout (float): seconds granted to the polite part.
    """
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()


def _stable_size(path: str, quiet_for: float = 3.0,
                 timeout: float = 60.0) -> int:
    """Wait until a file stops growing.

    Args:
        path (str): the file to watch.
        quiet_for (float, optional): seconds without a change. Defaults to 3.
        timeout (float, optional): give up after this. Defaults to 60.

    Returns:
        Its final size in bytes.
    """
    deadline = time.monotonic() + timeout
    last = -1
    unchanged_since = time.monotonic()
    while time.monotonic() < deadline:
        try:
            size = os.path.getsize(path)
        except OSError:
            size = -1
        if size != last:
            last = size
            unchanged_since = time.monotonic()
        elif time.monotonic() - unchanged_since >= quiet_for:
            break
        time.sleep(0.5)
    return max(last, 0)


def finish(tracer: Tracer, timeout: float = 600.0) -> str:
    """Wait for a tracer to close its trace after its target has been stopped.

    The caller stops the traced process -- for the allocation benchmark that is
    the robot test itself, which stops centengine once its workload is over.
    heaptrack then sees the process go, flushes what its interpreter still holds,
    compresses it and exits. That flush is not instant on a large trace, which is
    what the timeout is for.

    Args:
        tracer: what attach() returned.
        timeout (float, optional): seconds granted to the flush. Defaults to 600.

    Returns:
        The path of the closed trace.

    Raises:
        HeaptrackError: if the target is still running -- nobody stopped it -- or
            if heaptrack never finished.
    """
    try:
        tracer.proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        if _alive(tracer.target_pid):
            raise HeaptrackError(
                f"the traced process {tracer.target_pid} is still running: "
                "heaptrack only closes its trace when its target exits. Stop "
                "the process, or call force_stop() and accept that it dies.")
        raise HeaptrackError(
            f"heaptrack did not finish flushing {tracer.trace_path} within "
            f"{timeout:.0f}s; see {tracer.log_path}")
    _stable_size(tracer.trace_path)
    return tracer.trace_path


def force_stop(tracer: Tracer, timeout: float = 120.0) -> str:
    """Remove the injection from a target that cannot be stopped -- killing it.

    On heaptrack 1.5.0 the ``heaptrack_stop()`` this runs trips an assertion
    inside libheaptrack and aborts the debuggee. It is still the least bad way
    out when a trace has to be salvaged from a process that must not be left
    traced, and it is exactly what the heaptrack script does in its own cleanup.

    Args:
        tracer: what attach() returned.
        timeout (float, optional): seconds granted to each step. Defaults to 120.

    Returns:
        The path of the trace, closed but from an aborted process.
    """
    if _alive(tracer.target_pid):
        try:
            subprocess.run(_gdb_stop_argv(tracer.target_pid),
                           capture_output=True, timeout=timeout, check=False)
        except subprocess.TimeoutExpired:
            pass
    if tracer.proc.poll() is None:
        try:
            tracer.proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            for sig in (signal.SIGTERM, signal.SIGKILL):
                try:
                    os.kill(tracer.proc.pid, sig)
                    tracer.proc.wait(timeout=30)
                    break
                except (ProcessLookupError, subprocess.TimeoutExpired):
                    continue
    _stable_size(tracer.trace_path)
    return tracer.trace_path


def _parse_size(text: str) -> float:
    """Turn a size printed by heaptrack_print into bytes.

    Args:
        text (str): e.g. "969.93K", "5.92M", "575B".

    Returns:
        The value in bytes.
    """
    text = text.strip()
    factors = {"B": 1, "K": 1024, "M": 1024 ** 2,
               "G": 1024 ** 3, "T": 1024 ** 4}
    factor = factors.get(text[-1:].upper())
    if factor is None:
        return float(text)
    return float(text[:-1]) * factor


def summary(trace_path: str, timeout: float = 1800.0) -> dict:
    """Read the scalar results out of a trace.

    The per-stack attribution is not returned: it is a report to read, not a
    figure to compare, and write_report() puts it next to the trace.

    Args:
        trace_path (str): the trace file.
        timeout (float, optional): seconds granted to heaptrack_print, which has
            to resolve the symbols of a large binary. Defaults to 1800.

    Returns:
        A dict of metric name to value, ready for the store.

    Raises:
        HeaptrackError: if heaptrack_print fails or its summary cannot be read.
    """
    out = subprocess.run(["heaptrack_print", "-f", trace_path,
                          "--print-peaks=0", "--print-allocators=0",
                          "--print-temporary=0"],
                         capture_output=True, text=True, timeout=timeout)
    if out.returncode != 0:
        raise HeaptrackError(
            f"heaptrack_print failed on {trace_path}: {out.stderr.strip()}")

    metrics = {}
    for line in out.stdout.splitlines():
        key, _, value = line.partition(":")
        key = key.strip()
        value = value.strip()
        if not value:
            continue
        try:
            if key == "total runtime":
                metrics["runtime_s"] = float(value.rstrip("s."))
            elif key == "calls to allocation functions":
                # "330480000 (5708166/s)" -- the rate is derivable, keep the count.
                metrics["alloc_calls"] = float(value.split()[0])
            elif key == "temporary memory allocations":
                metrics["temporary_allocs"] = float(value.split()[0])
            elif key == "peak heap memory consumption":
                metrics["peak_heap_bytes"] = _parse_size(value)
            elif key.startswith("peak RSS"):
                metrics["peak_rss_bytes"] = _parse_size(value)
            elif key == "total memory leaked":
                metrics["leaked_bytes"] = _parse_size(value)
        except ValueError:
            continue

    if "alloc_calls" not in metrics:
        raise HeaptrackError(
            f"no allocation count in the heaptrack_print output for "
            f"{trace_path}; the trace is probably truncated")
    return metrics


def write_report(trace_path: str, report_path: str, top: int = 20,
                 timeout: float = 1800.0) -> str:
    """Write the per-stack attribution next to the trace.

    Args:
        trace_path (str): the trace file.
        report_path (str): where to write the report.
        top (int, optional): how many allocators to report. Defaults to 20.
        timeout (float, optional): seconds granted to heaptrack_print. Defaults
            to 1800.

    Returns:
        The report path.
    """
    out = subprocess.run(["heaptrack_print", "-f", trace_path,
                          "--print-allocators=1", "--print-peaks=0",
                          "--print-temporary=1", f"--peak-limit={top}"],
                         capture_output=True, text=True, timeout=timeout)
    with open(report_path, "w") as f:
        f.write(out.stdout)
        if out.stderr.strip():
            f.write("\n--- stderr ---\n")
            f.write(out.stderr)
    return report_path


def diff(trace_a: str, trace_b: str, timeout: float = 3600.0) -> str:
    """Compare two traces stack by stack.

    Args:
        trace_a (str): the reference trace.
        trace_b (str): the trace to compare to it.
        timeout (float, optional): seconds granted to heaptrack_print. Defaults
            to 3600, twice the single-trace budget since it reads both.

    Returns:
        What heaptrack_print reports, where a negative cost is an improvement of
        B over A. This is the figure to read when two binaries are compared: a
        difference in totals says something changed, this says where.
    """
    out = subprocess.run(["heaptrack_print", "-f", trace_b, "-d", trace_a],
                         capture_output=True, text=True, timeout=timeout)
    return out.stdout if out.returncode == 0 else \
        f"heaptrack_print --diff failed: {out.stderr.strip()}"

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
"""The entry point of every benchmark in this directory.

    ./bench.py list
    ./bench.py run engine-config-load --sizes 1000,10000,50000
    ./bench.py probe --duration 600
    ./bench.py show --label dt-broker
    ./bench.py compare 25.10 dt-broker

Results always land in the same SQLite store (``results/bench.db``), so two
campaigns can be compared months apart, and a run measured on a customer
installation can be imported next to one measured in the container.

Two rules the tool enforces rather than documents: a benchmark refuses to run on
a modified working tree, because nobody can reproduce the binary it measured, and
a comparison never silently pairs runs whose parameters differ.
"""

import argparse
import datetime
import json
import os
import re
import shutil
import signal
import statistics
import subprocess
import sys
import time
from typing import Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import benchdb  # noqa: E402
import benchenv  # noqa: E402
import engine_config_gen  # noqa: E402
import procstat  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DB = os.path.join(HERE, "results", "bench.db")

# What "probe" watches by default, and how each process is told apart. The two
# cbd share a binary, so only their configuration file distinguishes them.
DEFAULT_TARGETS = (
    ("centengine", "centengine", None),
    ("central-broker", "cbd", "central-broker"),
    ("central-rrd", "cbd", "central-rrd"),
)

BENCHES = {
    "engine-config-load": "Cost of reading and applying an Engine configuration, "
    "over a range of sizes. No daemon, no database.",
    "alloc": "Heap allocations of centengine on the check path, per profile "
             "(EALLOC1 passive, EALLOC2 active, EALLOC3 active with a real "
             "command line). Drives robot and heaptrack. Container only.",
}

# Benchmarks that are robot tests rather than subcommands, listed because they
# file their results in the same store: someone reading "bench.py list" and
# finding two entries would reasonably conclude the other three do not exist.
ROBOT_BENCHES = {
    "engine-startup": ("startup_bench.robot",
                       "From process start to the event loop, phase by phase, "
                       "in the three shapes a configuration can take."),
    "load": ("collect_load_bench.robot",
             "What the three daemons cost in steady state, on a load the test "
             "builds itself: active checks, or passive results at a fixed rate."),
    "rrd-retention": ("rrd_retention_bench.robot",
                      "Throughput and merge latency of the RRD retention "
                      "buffer, on back-dated metrics."),
}

# The sentinels alloc_bench.robot synchronises on. Same paths, on purpose: they
# are the contract between the test and this driver.
ALLOC_READY = "/tmp/bench-alloc.ready"
ALLOC_GO = "/tmp/bench-alloc.go"
ALLOC_DONE = "/tmp/bench-alloc.done"
ALLOC_PROFILES = ("EALLOC1", "EALLOC2", "EALLOC3")


_MAIN_EPILOG = """\
examples:
  ./bench.py list                                  what can be run
  ./bench.py run engine-config-load                a benchmark, filed under the branch name
  ./bench.py probe --duration 600                  measure daemons already running
  ./bench.py show --label dt-broker                what a campaign holds
  ./bench.py compare 25.10 dt-broker               the verdict

Each command has its own options, and "run" has one group per benchmark:
  ./bench.py run -h        ./bench.py probe -h        ./bench.py compare -h
"""


def _run_epilog() -> str:
    """Build the help text listing what "run" can run.

    Written out rather than left to the BENCH argument's help line: someone
    typing "./bench.py run -h" is asking what the benchmarks *are*, and a bare
    list of five names does not answer that.

    Returns:
        The epilog of the run subcommand.
    """
    lines = ["the benchmarks:"]
    for name, description in sorted(BENCHES.items()):
        lines.append(f"  {name}")
        lines.append(f"      {description}")
    for name, (robot_file, description) in sorted(ROBOT_BENCHES.items()):
        lines.append(f"  {name}")
        lines.append(f"      {description}")
        lines.append(f"      a robot test: benchmarks/{robot_file}")
    lines += [
        "",
        "examples:",
        "  ./bench.py run engine-config-load --sizes 10000,50000 --repeat 5",
        "  ./bench.py run alloc --profile EALLOC2",
        "  ./bench.py run engine-startup --test BENCH_START_PROTO",
        "  ./bench.py run load --var duration:1800 --var nb_hosts:100",
        "  ./bench.py run rrd-retention --var N_METRICS:10",
        "",
        "every run is filed in the store under a label -- the git branch unless",
        "--label says otherwise -- and compared later with ./bench.py compare.",
    ]
    return "\n".join(lines) + "\n"


def _log(message: str = ""):
    """Print a line of progress, flushed so a long run stays readable.

    Args:
        message (str, optional): what to print. Defaults to an empty line.
    """
    print(message, flush=True)


def _die(message: str):
    """Print an error and leave with a failure status.

    Args:
        message (str): what went wrong.
    """
    print(f"bench.py: {message}", file=sys.stderr, flush=True)
    sys.exit(1)


def _resolve_label(label: Optional[str], env: dict) -> str:
    """Decide under which name a campaign is stored.

    Args:
        label (str, optional): what the caller asked for, if anything.
        env (dict): the environment description, holding the git branch.

    Returns:
        The label to use: the given one, else the current branch, else "unnamed".
    """
    return label or env.get("git_branch") or "unnamed"


def _guard_dirty(env: dict, allow_dirty: bool):
    """Refuse to measure a working tree that nobody can reproduce.

    Args:
        env (dict): the environment description.
        allow_dirty (bool): whether the caller accepted the risk explicitly.
    """
    if env.get("git_dirty") and not allow_dirty:
        _die("the working tree has uncommitted changes: the measurement would "
             "not be reproducible. Commit, stash, or pass --allow-dirty.")


def _binary_notes(path: str) -> str:
    """Describe the measured binary well enough to recognise it later.

    Args:
        path (str): the binary.

    Returns:
        A one-line description carrying its size and modification time, which is
        what tells two builds of the same commit apart.
    """
    try:
        st = os.stat(path)
    except OSError:
        return f"binary={path} (unreadable)"
    mtime = datetime.datetime.fromtimestamp(st.st_mtime).replace(
        microsecond=0).isoformat()
    return f"binary={path} size={st.st_size} mtime={mtime}"


def _find_engine(explicit: Optional[str]) -> str:
    """Locate the centengine binary to measure.

    Args:
        explicit (str, optional): a path given on the command line.

    Returns:
        The path of the binary. The installed one wins over the build tree: it is
        the one the robot benchmarks run, so the figures stay comparable. Pass
        --engine to measure a build directly.
    """
    if explicit:
        if not os.access(explicit, os.X_OK):
            _die(f"not an executable: {explicit}")
        return explicit
    found = shutil.which("centengine")
    if found:
        return found
    root = benchenv.repo_root()
    if root:
        candidate = os.path.join(root, "build", "engine", "centengine")
        if os.access(candidate, os.X_OK):
            return candidate
    _die("centengine not found in PATH; pass --engine <path>")
    raise AssertionError  # unreachable, _die exits


def _key_values(items: Optional[list]) -> dict:
    """Turn repeated "key=value" arguments into a dict.

    Values that look like numbers are stored as numbers, so that a size reads as
    a size in the store rather than as a string -- and so that two runs written
    by different callers, one passing 50 and the other "50", still pair.

    Args:
        items (list, optional): the raw arguments.

    Returns:
        The parsed dict, empty if there was nothing.
    """
    out = {}
    for item in items or []:
        key, sep, value = item.partition("=")
        if not sep:
            _die(f"--param takes key=value, got '{item}'")
        key = key.strip()
        value = value.strip()
        try:
            out[key] = int(value)
        except ValueError:
            try:
                out[key] = float(value)
            except ValueError:
                out[key] = value
    return out


def _open_db(path: str) -> benchdb.BenchDB:
    """Open the result store.

    Args:
        path (str): path of the SQLite file.

    Returns:
        The opened store.
    """
    try:
        return benchdb.BenchDB(path)
    except Exception as e:  # sqlite3 errors, permissions, schema mismatch
        _die(f"cannot open {path}: {e}")
        raise AssertionError


# ---------------------------------------------------------------------------
# engine-config-load
# ---------------------------------------------------------------------------

def _checked_counts(output: str) -> dict:
    """Read back the object counts centengine reports after checking a config.

    Args:
        output (str): what "centengine -v" printed.

    Returns:
        A dict of the counts found, e.g. {"hosts": 500, "services": 10000}. Used
        as a sanity check: a run whose counts do not match what was generated
        measured something else than intended.
    """
    counts = {}
    for line in output.splitlines():
        line = line.strip()
        if line.startswith("Checked "):
            parts = line[len("Checked "):].rstrip(".").split(None, 1)
            if len(parts) == 2 and parts[0].isdigit():
                counts[parts[1].replace(" ", "_")] = int(parts[0])
        # Warnings are worth keeping as a result of their own: they are emitted
        # once per object, so a change making resolve() complain about every
        # service turns the benchmark into a measurement of spdlog without
        # anything in the timings saying so.
        elif line.startswith("Total Warnings:"):
            counts["config_warnings"] = int(line.split(":")[1].strip())
        elif line.startswith("Total Errors:"):
            counts["config_errors"] = int(line.split(":")[1].strip())
    return counts


def _startup_phases(output: str) -> dict:
    """Read the phase durations Engine printed about its own startup.

    Engine emits them whatever it was asked to do, --verify-config included, so
    the daemon-free benchmark gets the same breakdown a real startup would give.
    Without them all this benchmark could report is a total.

    Args:
        output (str): what centengine printed.

    Returns:
        A dict of "phase.<name>_ms" to milliseconds, empty on a binary that
        predates the instrumentation.
    """
    phases = {}
    for name, value in re.findall(
            r"Startup timing: (\S+) = (\d+) ms", output):
        phases[f"phase.{name}_ms"] = float(value)
    return phases


def bench_config_load(args, db: benchdb.BenchDB, env: dict, label: str) -> int:
    """Measure the cost of reading and applying a configuration, by size.

    One run per size, so a sweep produces a curve rather than a single number.
    Each size is measured several times and the median is kept: the first
    iteration pays for a cold page cache on the freshly written .cfg files, and
    that has nothing to do with the code under measurement.

    Note that --verify-config does parse, expand, resolve *and* apply
    (engine/src/main.cc), so what is measured here is the whole configuration
    path, not just its parsing. Splitting it per phase needs the startup timing
    logs; until then, this is the total.

    Args:
        args: the parsed command line.
        db: the result store.
        env (dict): the environment description.
        label (str): the campaign name.

    Returns:
        A process exit status.
    """
    engine = _find_engine(args.engine)
    modes = {"verify": ["-v"], "test-scheduling": ["-s"]}
    if args.mode == "both":
        chosen = ["verify", "test-scheduling"]
    else:
        chosen = [args.mode]

    try:
        sizes = [int(s) for s in args.sizes.split(",") if s.strip()]
    except ValueError:
        _die(f"--sizes takes whole numbers of services, got '{args.sizes}'")
        raise AssertionError
    if not sizes:
        _die("--sizes is empty")

    workdir = args.workdir or os.path.join(
        HERE, "results", "engine-config-load-tmp")
    os.makedirs(workdir, mode=0o775, exist_ok=True)
    notes = _binary_notes(engine)
    _log(f"== engine-config-load: {label} ==")
    _log(f"   {notes}")
    _log(f"   {len(sizes)} size(s), {args.repeat} repetition(s), "
         f"mode(s) {', '.join(chosen)}")

    status = 0
    for services in sizes:
        hosts = max(1, -(-services // args.services_by_host))
        directory = os.path.join(workdir, f"config-{services}")
        generated = engine_config_gen.generate(
            directory, hosts=hosts,
            services_by_host=args.services_by_host,
            commands=args.commands, timezone=args.timezone)
        _log(f"\n-- {generated.services} services on {generated.hosts} hosts "
             f"({directory})")

        for mode in chosen:
            # The binary being measured is deliberately absent: comparing a
            # build tree against an installed one is the whole point of
            # compare, and params_json is what pairs two runs. It is recorded
            # in the notes instead.
            params = {"services": generated.services,
                      "hosts": generated.hosts,
                      "services_by_host": args.services_by_host,
                      "commands": args.commands,
                      "repeat": args.repeat}
            run_id = db.start_run(label=label, bench="engine-config-load",
                                  variant=mode, started_at=benchenv.now_iso(),
                                  params=params, env=env, notes=notes)
            costs = []
            phase_costs: list = []
            failed = False
            last_counts: dict = {}
            for iteration in range(1, args.repeat + 1):
                cost = procstat.measure_command(
                    [engine, *modes[mode], "-f", generated.main_file])
                if cost.exit_code != 0:
                    failed = True
                    _log(f"   {mode} #{iteration}: FAILED "
                         f"(exit {cost.exit_code})")
                    tail = "\n".join(cost.output.strip().splitlines()[-15:])
                    _log(tail)
                    break
                last_counts = _checked_counts(cost.output)
                checked = last_counts.get("services")
                if checked is not None and checked != generated.services:
                    _log(f"   {mode} #{iteration}: engine checked {checked} "
                         f"services, {generated.services} were generated")
                if last_counts.get("config_warnings"):
                    _log(f"   {mode} #{iteration}: "
                         f"{last_counts['config_warnings']} configuration "
                         "warning(s) -- one per object means the run is "
                         "measuring the logger too")
                costs.append(cost)
                phases = _startup_phases(cost.output)
                phase_costs.append(phases)
                db.add_series(run_id, iteration, mode,
                              {"wall_s": cost.wall_s,
                               "user_s": cost.user_s,
                               "sys_s": cost.sys_s,
                               "maxrss_kb": cost.maxrss_kb,
                               **phases})
                _log(f"   {mode} #{iteration}: {cost.wall_s:.2f}s wall, "
                     f"{cost.user_s:.2f}s user, {cost.sys_s:.2f}s sys, "
                     f"{cost.maxrss_kb / 1024:.0f}MB peak")
                if phases:
                    _log("      " + ", ".join(
                        f"{name[len('phase.'):-len('_ms')]} {value:.0f}ms"
                        for name, value in phases.items()))

            if failed or not costs:
                db.finish_run(run_id, benchenv.now_iso(), status="failed")
                status = 1
                continue

            walls = [c.wall_s for c in costs]
            cpus = [c.user_s + c.sys_s for c in costs]
            metrics = {
                # Wall clock is what an operator waits, but it carries a fixed
                # cost of about two tenths of a second: centengine tears its
                # broker protocols down before leaving, whatever the size of the
                # configuration. cpu_s does not, so it is the figure to watch
                # when comparing two binaries.
                "wall_s": statistics.median(walls),
                "wall_min_s": min(walls),
                "wall_max_s": max(walls),
                "cpu_s": statistics.median(cpus),
                "user_s": statistics.median([c.user_s for c in costs]),
                "sys_s": statistics.median([c.sys_s for c in costs]),
                "maxrss_kb": statistics.median([c.maxrss_kb for c in costs]),
                "hosts": generated.hosts,
                "services": generated.services,
                # The per-service cost is what makes two sizes comparable: a
                # linear configuration path keeps it flat, anything quadratic
                # shows up here long before the totals look suspicious.
                "ms_per_service": statistics.median(walls) * 1000
                / generated.services,
                "cpu_ms_per_service": statistics.median(cpus) * 1000
                / generated.services,
            }
            units = {"wall_s": "s", "wall_min_s": "s", "wall_max_s": "s",
                     "cpu_s": "s", "user_s": "s", "sys_s": "s",
                     "maxrss_kb": "kB", "ms_per_service": "ms",
                     "cpu_ms_per_service": "ms"}
            for name in ("config_warnings", "config_errors"):
                if name in last_counts:
                    metrics[name] = last_counts[name]
            # The phase breakdown Engine printed about itself. Median as well,
            # and only over the iterations that reported a given phase: an
            # instrumented binary reports them all, an older one none, and a
            # mixture would mean the run measured two different binaries.
            for name in {n for phases in phase_costs for n in phases}:
                values = [p[name] for p in phase_costs if name in p]
                metrics[name] = statistics.median(values)
                units[name] = "ms"
            db.add_metrics(run_id, metrics, units)
            db.finish_run(run_id, benchenv.now_iso(), status="ok")
            _log(f"   {mode}: median {metrics['wall_s']:.2f}s wall, "
                 f"{metrics['cpu_s']:.2f}s cpu, "
                 f"{metrics['cpu_ms_per_service']:.3f} cpu-ms/service, "
                 f"peak {metrics['maxrss_kb'] / 1024:.0f}MB  [run {run_id}]")

    if not args.keep:
        shutil.rmtree(workdir, ignore_errors=True)
    else:
        _log(f"\nGenerated configurations kept in {workdir}")
    return status


# ---------------------------------------------------------------------------
# alloc
# ---------------------------------------------------------------------------

def _clear_sentinels():
    """Remove the alloc sentinels, whoever left them behind.

    A file left by an interrupted run would release the next one before its
    time, which is the one failure mode of a file-based handshake.
    """
    for path in (ALLOC_READY, ALLOC_GO, ALLOC_DONE):
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass


def _wait_for_sentinel(path: str, robot: subprocess.Popen, timeout: float,
                       what: str) -> str:
    """Wait for the robot test to publish a sentinel.

    Args:
        path (str): the sentinel to wait for.
        robot: the running robot process, watched so that a test dying does not
            leave the driver waiting for a file nobody will ever write.
        timeout (float): seconds to wait.
        what (str): what is being waited for, for the error message.

    Returns:
        The content of the sentinel.

    Raises:
        RuntimeError: if robot exits first, or the wait times out.
    """
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if os.path.exists(path):
            with open(path, "r") as f:
                return f.read().strip()
        if robot.poll() is not None:
            raise RuntimeError(
                f"the robot test exited (status {robot.returncode}) before "
                f"{what}")
        time.sleep(1)
    raise RuntimeError(f"timed out after {timeout:.0f}s waiting for {what} "
                       f"({path})")


def bench_alloc(args, db: benchdb.BenchDB, env: dict, label: str) -> int:
    """Count the heap allocations of centengine on the check path.

    The benchmark itself is a robot test: it builds the configuration, starts the
    daemons, warms centengine up and runs the workload. What this driver adds is
    the part a human used to do by hand -- attaching heaptrack once centengine is
    warm, and reading the trace once the workload is over -- which is what makes
    the profile reproducible between two binaries.

    It has to run where the daemons run, so inside the test container.

    Args:
        args: the parsed command line.
        db: the result store.
        env (dict): the environment description.
        label (str): the campaign name.

    Returns:
        A process exit status.
    """
    import heaptrack_tools

    if not heaptrack_tools.available():
        _die("heaptrack and heaptrack_print are needed; this benchmark runs "
             "inside the test container")
    if not shutil.which("robot"):
        _die("robot is not on the PATH: activate the robotframework virtualenv "
             "(. robotframework/bin/activate)")

    tests_dir = os.path.dirname(HERE)
    profiles = list(ALLOC_PROFILES) if args.profile == "all" else [
        args.profile]
    engine = args.engine or "/usr/sbin/centengine"
    status = 0

    for profile in profiles:
        outdir = os.path.join(HERE, "results", label, "alloc", profile)
        os.makedirs(outdir, mode=0o775, exist_ok=True)
        prefix = os.path.join(outdir, profile)
        _log(f"\n== alloc/{profile}: {label} ==")
        _log(f"   robot output and trace in {outdir}")
        _clear_sentinels()

        robot_log = open(os.path.join(outdir, "robot.log"), "w")
        robot = subprocess.Popen(
            ["robot", "--outputdir", outdir, "--test", profile,
             "benchmarks/alloc_bench.robot"],
            cwd=tests_dir, stdout=robot_log, stderr=subprocess.STDOUT)
        robot_log.close()

        run_id = db.start_run(
            label=label, bench="alloc", variant=profile,
            started_at=benchenv.now_iso(), params={"profile": profile},
            env=env, notes=_binary_notes(engine))
        tracer = None
        try:
            pid_text = _wait_for_sentinel(
                ALLOC_READY, robot, args.ready_timeout,
                "centengine to be warm")
            pid = int(pid_text.split()[0])
            _log(f"   centengine is warm, pid {pid}; attaching heaptrack")
            tracer = heaptrack_tools.attach(pid, prefix,
                                            timeout=args.attach_timeout)
            _log(f"   attached, tracing into {tracer.trace_path}")
            # Releasing the test only now: everything the workload does has to
            # be inside the trace, or the two binaries would not be compared on
            # the same work.
            with open(ALLOC_GO, "w") as f:
                f.write(f"{tracer.proc.pid}\n")

            done = _wait_for_sentinel(ALLOC_DONE, robot, args.done_timeout,
                                      "the workload to finish")
            _log(f"   workload over: {done}")
            # The test stops centengine right after writing that sentinel, which
            # is what makes heaptrack flush and exit: 1.5.0 cannot be detached
            # from a live process.
            trace = heaptrack_tools.finish(tracer, timeout=args.flush_timeout)
            size_mb = os.path.getsize(trace) / (1024 * 1024)
            _log(f"   trace closed, {size_mb:.1f}MB; reading it")

            metrics = heaptrack_tools.summary(trace)
            report = heaptrack_tools.write_report(
                trace, os.path.join(outdir, f"{profile}.report.txt"))
            units = {"runtime_s": "s", "peak_heap_bytes": "B",
                     "peak_rss_bytes": "B", "leaked_bytes": "B"}
            db.add_metrics(run_id, metrics, units)
            # The trace path travels with the run: "compare --diff" needs it to
            # put two traces side by side, which is the only way to see *where*
            # the allocations moved rather than just how many there are.
            db.append_note(run_id, f"trace={os.path.abspath(trace)}")
            db.finish_run(run_id, benchenv.now_iso(), status="ok")
            _print_metrics(metrics, units)
            _log(f"   per-stack attribution in {report}")
            _log(f"   [run {run_id}]")
        except (RuntimeError, ValueError, OSError) as e:
            db.finish_run(run_id, benchenv.now_iso(), status="failed")
            _log(f"   FAILED: {e}")
            status = 1
            if tracer is not None and tracer.proc.poll() is None:
                # Leaving a tracer attached would keep centengine traced for the
                # rest of the session, so it goes -- even though that kills it.
                _log("   removing the injection from centengine (this kills it)")
                heaptrack_tools.force_stop(tracer)
        finally:
            # Releasing the test whatever happened: without this it would sit on
            # its go sentinel until its own ten minute timeout.
            try:
                os.unlink(ALLOC_GO)
            except FileNotFoundError:
                pass
            try:
                robot.wait(timeout=args.teardown_timeout)
            except subprocess.TimeoutExpired:
                robot.terminate()
                _log("   the robot test had to be terminated")
                status = 1
            if robot.returncode not in (0, None):
                _log(f"   note: robot exited with status {robot.returncode}, "
                     f"see {outdir}/robot.log and {outdir}/log.html")
            _clear_sentinels()

    return status


# ---------------------------------------------------------------------------
# the robot-driven benchmarks
# ---------------------------------------------------------------------------

def bench_robot(name: str, args, env: dict, label: str) -> int:
    """Run one of the benchmarks that are robot tests.

    A thin wrapper, and worth having: it gives every benchmark the same entry
    point, resolves the campaign name the same way, and applies the same refusal
    to measure a modified working tree -- which a bare "robot benchmarks/..."
    does not. Robot's own output goes straight to the terminal, since these runs
    take minutes and watching them is the point.

    Args:
        name (str): the benchmark, a key of ROBOT_BENCHES.
        args: the parsed command line.
        env (dict): the environment description, unused here -- the test files
            its own run, with its own environment.
        label (str): the campaign name, passed to the test as a robot variable.

    Returns:
        A process exit status, robot's own.
    """
    del env
    if not shutil.which("robot"):
        _die("robot is not on the PATH: activate the robotframework virtualenv "
             "(. robotframework/bin/activate)")

    robot_file, _description = ROBOT_BENCHES[name]
    tests_dir = os.path.dirname(HERE)
    outdir = os.path.join(HERE, "results", label, name)
    os.makedirs(outdir, mode=0o775, exist_ok=True)

    argv = ["robot", "--outputdir", outdir, "-v", f"label:{label}"]
    for pair in args.var or []:
        if ":" not in pair:
            _die(f"--var takes name:value, got '{pair}'")
        argv += ["-v", pair]
    if args.test:
        argv += ["--test", args.test]
    argv.append(os.path.join("benchmarks", robot_file))

    _log(f"== {name}: {label} ==")
    _log(f"   {' '.join(argv)}")
    _log(f"   robot output in {outdir}")
    result = subprocess.run(argv, cwd=tests_dir)
    if result.returncode != 0:
        _log(f"\n   robot exited with status {result.returncode}: see "
             f"{outdir}/log.html")
        return 1
    _log(f"\n   filed under label '{label}'; ./bench.py show --label {label} "
         f"--bench {name}")
    return 0


# ---------------------------------------------------------------------------
# probe
# ---------------------------------------------------------------------------

def _read_engine_stats(engine_config: str) -> Optional[dict]:
    """Ask centenginestats for the check counters.

    Args:
        engine_config (str): main Engine configuration file.

    Returns:
        A dict with the 5 minute host and service check counts and the average
        active service latency, or None when centenginestats is unavailable. It
        is the denominator: without it the CPU totals are still comparable
        between two runs carrying the same configuration, but the cost per check
        cannot be derived.
    """
    binary = shutil.which("centenginestats")
    if not binary or not os.access(engine_config, os.R_OK):
        return None
    cost = procstat.measure_command([binary, "-c", engine_config])
    if cost.exit_code != 0:
        return None
    stats = {}
    for line in cost.output.splitlines():
        key, _, value = line.partition(":")
        fields = [f.strip() for f in value.split("/")]
        try:
            if "Active Host Checks Last 1/5/15 min" in key:
                stats["host_5min"] = float(fields[1])
            elif "Active Service Checks Last 1/5/15 min" in key:
                stats["service_5min"] = float(fields[1])
            elif "Active Service Latency" in key:
                # min / max / average, and the average carries the unit.
                stats["latency_avg_s"] = float(
                    fields[2].split()[0])
        except (IndexError, ValueError):
            continue
    return stats or None


def cmd_probe(args) -> int:
    """Measure what the running collect daemons cost, over a window.

    It measures processes someone else started, which is what makes it usable
    both on a robot benchmark and on a real installation -- it replaces the
    bench-load.sh it was ported from, whose counters it was cross-checked
    against, integer for integer, before that script was retired.

    Args:
        args: the parsed command line.

    Returns:
        A process exit status.
    """
    env = benchenv.describe()
    label = _resolve_label(args.label, env)
    db = _open_db(args.db)

    probes = []
    for name, comm, match in DEFAULT_TARGETS:
        pids = procstat.find_pids(comm, match)
        if not pids:
            _die(f"{name} is not running (looking for comm '{comm}'"
                 + (f" with '{match}' in its command line)" if match else ")"))
        if len(pids) > 1:
            _die(f"several processes match {name} ({pids}); refusing to guess")
        probes.append((name, pids[0]))

    _log(f"== probe: {label} ==")
    for name, pid in probes:
        _log(f"   {name}: pid {pid}")

    if args.warmup > 0:
        _log(f"   warm-up: ignoring the first {args.warmup}s -- broker replays "
             "its retention and loads its cache at startup")
        time.sleep(args.warmup)

    params = {"duration": args.duration, "warmup": args.warmup,
              "interval": args.interval,
              "targets": [name for name, _ in probes]}
    # What the caller says about the load it built. Without it two windows
    # measuring completely different things -- an active profile and a passive
    # one -- would look like the same measured point and compare would happily
    # pair them.
    params.update(_key_values(args.param))
    run_id = db.start_run(label=label, bench="load", variant=args.variant,
                          started_at=benchenv.now_iso(), params=params, env=env,
                          notes=args.notes)

    opened = [procstat.ProcessProbe(name, pid) for name, pid in probes]
    machine_busy0, machine_total0 = procstat.read_machine()
    prev_busy, prev_total = machine_busy0, machine_total0
    stats_samples = []

    _log(f"   measuring for {args.duration}s, sampling every {args.interval}s")
    started = time.monotonic()
    aborted = False
    try:
        while time.monotonic() - started < args.duration:
            time.sleep(min(args.interval, args.duration -
                           (time.monotonic() - started)))
            elapsed = time.monotonic() - started
            for probe in opened:
                values = probe.sample(args.interval)
                if values is None:
                    _log(f"   {probe.name} vanished")
                    continue
                db.add_series(run_id, round(elapsed), probe.name, values)
            busy, total = procstat.read_machine()
            machine_pct = ((busy - prev_busy) * 100 / (total - prev_total)
                           if total > prev_total else 0.0)
            prev_busy, prev_total = busy, total
            db.add_series(run_id, round(elapsed), "machine",
                          {"cpu_pct": machine_pct})
            stats = _read_engine_stats(args.engine_config)
            if stats:
                stats_samples.append(stats)
    except KeyboardInterrupt:
        aborted = True
        _log("\n   interrupted: keeping what was measured so far")

    elapsed = time.monotonic() - started
    machine_busy1, machine_total1 = procstat.read_machine()

    # A daemon restarting mid-window resets its counters, so the whole run is
    # void: the CPU deltas would be nonsense and must not be stored as if they
    # meant something.
    restarted = [p.name for p in opened if p.restarted()]
    if restarted:
        db.finish_run(run_id, benchenv.now_iso(), status="void")
        _die(f"{', '.join(restarted)} restarted during the window: run {run_id} "
             "is void")

    metrics = {"window_s": elapsed}
    units = {"window_s": "s"}
    collect_cpu = 0.0
    for probe in opened:
        cpu = probe.total_cpu_s()
        collect_cpu += cpu
        metrics[f"{probe.name}.cpu_total_s"] = cpu
        metrics[f"{probe.name}.cpu_avg_pct"] = cpu / elapsed * 100
        metrics[f"{probe.name}.rss_start_kb"] = probe.rss_first_kb
        metrics[f"{probe.name}.rss_end_kb"] = probe.rss_last_kb
        metrics[f"{probe.name}.rss_max_kb"] = probe.rss_max_kb
        units[f"{probe.name}.cpu_total_s"] = "s"
        units[f"{probe.name}.cpu_avg_pct"] = "%"
        for suffix in ("rss_start_kb", "rss_end_kb", "rss_max_kb"):
            units[f"{probe.name}.{suffix}"] = "kB"
    metrics["collect.cpu_total_s"] = collect_cpu
    # Percentage of *one* core, which is how anyone reads the CPU of a process.
    metrics["collect.cpu_avg_pct"] = collect_cpu / elapsed * 100
    units["collect.cpu_total_s"] = "s"
    units["collect.cpu_avg_pct"] = "%"

    machine_delta = machine_total1 - machine_total0
    if machine_delta > 0:
        # Percentage of the *whole machine*, all cores together: /proc/stat
        # aggregates every CPU. So this is not the same unit as the figures
        # above, and comparing them directly -- which the bash probe this was
        # ported from did -- overstates the share of the collect by the number
        # of cores. Hence the division below.
        machine_pct = (machine_busy1 - machine_busy0) * 100 / machine_delta
        metrics["machine.cpu_avg_pct"] = machine_pct
        units["machine.cpu_avg_pct"] = "%"
        cpus = env.get("cpu_count") or 1
        if machine_pct > 0:
            metrics["collect.share_pct"] = (
                collect_cpu / elapsed * 100 / cpus) / machine_pct * 100
            units["collect.share_pct"] = "%"

    if stats_samples:
        host_rate = statistics.mean(
            s.get("host_5min", 0) for s in stats_samples) / 5
        service_rate = statistics.mean(
            s.get("service_5min", 0) for s in stats_samples) / 5
        checks = (host_rate + service_rate) * (elapsed / 60)
        # Named "active" because that is all centenginestats counts. In a purely
        # passive profile these stay at zero, and a cost per check derived from
        # them would be meaningless -- hence the floor below rather than a bare
        # "checks > 0", which would have divided a whole window of CPU by the one
        # or two host checks that slipped through.
        metrics["active_checks.host_per_min"] = host_rate
        metrics["active_checks.service_per_min"] = service_rate
        metrics["active_checks.total"] = checks
        if checks >= 10:
            metrics["cpu_ms_per_active_check"] = collect_cpu * 1000 / checks
            units["cpu_ms_per_active_check"] = "ms"
        latencies = [s["latency_avg_s"]
                     for s in stats_samples if "latency_avg_s" in s]
        if latencies:
            metrics["latency_avg_s"] = statistics.mean(latencies)
            units["latency_avg_s"] = "s"

    db.add_metrics(run_id, metrics, units)
    db.finish_run(run_id, benchenv.now_iso(),
                  status="aborted" if aborted else "ok")
    _log("")
    _print_metrics(metrics, units)
    _log(f"\nrun {run_id} in {args.db}")
    if not stats_samples:
        _log("centenginestats unavailable, no denominator: the CPU totals stay "
             "comparable as long as both runs carry the same configuration.")
    db.close()
    return 0


# ---------------------------------------------------------------------------
# reporting
# ---------------------------------------------------------------------------

def _print_metrics(metrics: dict, units: Optional[dict] = None):
    """Print a metric table, widest name first column.

    Args:
        metrics (dict): name to value.
        units (dict, optional): name to unit. Defaults to None.
    """
    units = units or {}
    if not metrics:
        _log("   (no metric)")
        return
    width = max(len(name) for name in metrics)
    for name in sorted(metrics):
        unit = units.get(name) or ""
        _log(f"   {name:<{width}}  {metrics[name]:>12.3f} {unit}")


def _describe_run(row) -> str:
    """Summarise a run on one line.

    Args:
        row: a row of the run table.

    Returns:
        Its identity: id, benchmark, variant, parameters and state.
    """
    params = json.loads(row["params_json"] or "{}")
    # Every parameter, not a hand-picked few: they are what pairs two runs when
    # comparing campaigns, so hiding one hides part of the run's identity. An
    # allowlist did that at first, and a benchmark whose parameters were not on
    # it -- the RRD one, with metrics and old_points -- displayed as if it had
    # none. Only the long ones are trimmed, and visibly so.
    shown = " ".join(f"{k}={v}" for k, v in sorted(params.items()))
    if len(shown) > 90:
        shown = shown[:87] + "..."
    dirty = " (dirty)" if row["git_dirty"] else ""
    return (f"#{row['id']} {row['bench']}/{row['variant'] or '-'} "
            f"{shown} [{row['status']}] {row['started_at']} "
            f"{row['git_commit'] or '?'}{dirty}")


def cmd_show(args) -> int:
    """Show what the store holds.

    Args:
        args: the parsed command line.

    Returns:
        A process exit status.
    """
    db = _open_db(args.db)
    if args.run:
        row = db.run(args.run)
        if row is None:
            _die(f"no run #{args.run} in {args.db}")
            raise AssertionError
        _log(_describe_run(row))
        _log(f"   label      {row['label']}")
        _log(f"   host       {row['host']} "
             f"({row['container'] or 'bare machine'}, "
             f"{row['cpu_count']} CPU)")
        _log(f"   branch     {row['git_branch']} @ {row['git_commit']}")
        _log(f"   params     {row['params_json']}")
        if row["notes"]:
            _log(f"   notes      {row['notes']}")
        _log("")
        metrics = db.metrics(args.run)
        _print_metrics({k: v[0] for k, v in metrics.items()},
                       {k: v[1] for k, v in metrics.items() if v[1]})
        db.close()
        return 0

    if not args.label:
        rows = db.labels()
        if not rows:
            _log(f"{args.db} is empty")
        else:
            width = max(len(r["label"]) for r in rows)
            for r in rows:
                _log(f"{r['label']:<{width}}  {r['runs']:>4} run(s)  "
                     f"last {r['last']}  {r['benches']}")
        db.close()
        return 0

    rows = db.runs(label=args.label, bench=args.bench)
    if not rows:
        _log(f"no run for label '{args.label}'"
             + (f" and bench '{args.bench}'" if args.bench else ""))
        db.close()
        return 0
    for row in rows:
        _log(_describe_run(row))
        metrics = db.metrics(row["id"])
        if args.metrics:
            wanted = set(args.metrics.split(","))
            metrics = {k: v for k, v in metrics.items() if k in wanted}
        _print_metrics({k: v[0] for k, v in metrics.items()},
                       {k: v[1] for k, v in metrics.items() if v[1]})
        _log("")
    db.close()
    return 0


def cmd_compare(args) -> int:
    """Compare two campaigns, metric by metric.

    Args:
        args: the parsed command line.

    Returns:
        A process exit status.
    """
    db = _open_db(args.db)
    pairs = db.pair_runs(args.label_a, args.label_b, bench=args.bench)
    if not pairs:
        _log(f"nothing to compare: no run of '{args.label_a}' and "
             f"'{args.label_b}' measured the same thing")
        # Runs whose parameters differ are never paired silently, so say what
        # each label actually holds rather than leaving the user guessing.
        for label in (args.label_a, args.label_b):
            rows = db.runs(label=label, bench=args.bench)
            _log(f"\n{label}: {len(rows)} run(s)")
            for row in rows[:10]:
                _log(f"   {_describe_run(row)}")
        db.close()
        return 1

    for run_a, run_b in pairs:
        params = json.loads(run_a["params_json"] or "{}")
        _log(f"== {run_a['bench']}/{run_a['variant'] or '-'} "
             f"{json.dumps(params, sort_keys=True)}")
        _log(f"   {args.label_a} #{run_a['id']} ({run_a['git_commit']}) vs "
             f"{args.label_b} #{run_b['id']} ({run_b['git_commit']})")
        if run_a["cpu_count"] != run_b["cpu_count"] or \
                run_a["container"] != run_b["container"]:
            # Different machines burn CPU at different rates: comparing them is
            # meaningless, and the mismatch has to be said out loud.
            _log(f"   WARNING: measured on different environments "
                 f"({run_a['host']}/{run_a['cpu_count']} CPU vs "
                 f"{run_b['host']}/{run_b['cpu_count']} CPU)")
        metrics_a = db.metrics(run_a["id"])
        metrics_b = db.metrics(run_b["id"])
        names = sorted(set(metrics_a) | set(metrics_b))
        if not names:
            _log("   (no metric)")
            continue
        width = max(len(n) for n in names)
        _log(f"   {'metric':<{width}}  {args.label_a:>14}  "
             f"{args.label_b:>14}  {'delta':>10}")
        for name in names:
            a = metrics_a.get(name, (None, None))[0]
            b = metrics_b.get(name, (None, None))[0]
            if a is None or b is None:
                shown_a = "-" if a is None else f"{a:.3f}"
                shown_b = "-" if b is None else f"{b:.3f}"
                _log(f"   {name:<{width}}  {shown_a:>14}  {shown_b:>14}  "
                     f"{'n/a':>10}")
                continue
            delta = f"{(b - a) / a * 100:+.1f}%" if a else "n/a"
            _log(f"   {name:<{width}}  {a:>14.3f}  {b:>14.3f}  {delta:>10}")
        _log("")
        _compare_traces(run_a, run_b, args)
    db.close()
    return 0


def _trace_of(row) -> Optional[str]:
    """Find the heaptrack trace a run left behind.

    Args:
        row: a row of the run table.

    Returns:
        The trace path if the run recorded one and it is still there.
    """
    for token in (row["notes"] or "").split():
        if token.startswith("trace="):
            path = token[len("trace="):]
            return path if os.path.exists(path) else None
    return None


def _compare_traces(run_a, run_b, args):
    """Put two heaptrack traces side by side, or say how to.

    A difference in allocation counts says something changed; only the per-stack
    diff says where, which is the whole point of keeping the traces.

    Args:
        run_a: the reference run.
        run_b: the run compared to it.
        args: the parsed command line, for --diff.
    """
    trace_a, trace_b = _trace_of(run_a), _trace_of(run_b)
    if not trace_a or not trace_b:
        return
    if not getattr(args, "diff", False):
        _log("   both runs kept their trace; pass --diff for the per-stack "
             "comparison")
        return
    import heaptrack_tools
    _log("   per-stack difference (negative means fewer in "
         f"{run_b['label']}):")
    _log(heaptrack_tools.diff(trace_a, trace_b))


def cmd_import_csv(args) -> int:
    """Import a CSV written by the retired bench-load.sh.

    Kept because the windows measured with the bash probe -- on installations
    this tool never ran on -- are references worth having in the store.

    Args:
        args: the parsed command line.

    Returns:
        A process exit status.
    """
    if not os.access(args.csv, os.R_OK):
        _die(f"cannot read {args.csv}")
    db = _open_db(args.db)
    env = {"host": args.host, "container": None, "cpu_count": None,
           "git_branch": None, "git_commit": args.commit, "git_dirty": None}
    run_id = db.start_run(label=args.label, bench="load", variant="probe",
                          started_at=benchenv.now_iso(),
                          params={"imported_from": os.path.basename(args.csv)},
                          env=env,
                          notes=f"imported from {os.path.abspath(args.csv)}")
    per_target: dict[str, list] = {}
    rows = 0
    with open(args.csv, "r") as f:
        header = f.readline().strip().split(",")
        expected = ["target", "timestamp", "elapsed", "pid", "rss_kb",
                    "swap_kb", "cpu_pct"]
        if header != expected:
            db.finish_run(run_id, benchenv.now_iso(), status="failed")
            db.close()
            _die(f"unexpected CSV header: {header}")
        for line in f:
            fields = line.strip().split(",")
            if len(fields) != 7:
                continue
            target, _, elapsed, _, rss, swap, cpu = fields
            values = {}
            if rss:
                values["rss_kb"] = float(rss)
            if swap:
                values["swap_kb"] = float(swap)
            if cpu:
                values["cpu_pct"] = float(cpu)
            if not values:
                continue
            db.add_series(run_id, float(elapsed), target, values)
            per_target.setdefault(target, []).append(values)
            rows += 1

    metrics = {}
    units = {}
    for target, samples in per_target.items():
        cpus = [s["cpu_pct"] for s in samples if "cpu_pct" in s]
        rsss = [s["rss_kb"] for s in samples if "rss_kb" in s]
        if cpus:
            # Deliberately not named cpu_avg_pct: this is the mean of the
            # samples, not the cumulative average a probe computes from the tick
            # counters, and the two must never be compared as if they were the
            # same quantity.
            metrics[f"{target}.cpu_mean_sample_pct"] = statistics.mean(cpus)
            units[f"{target}.cpu_mean_sample_pct"] = "%"
        if rsss:
            metrics[f"{target}.rss_start_kb"] = rsss[0]
            metrics[f"{target}.rss_end_kb"] = rsss[-1]
            metrics[f"{target}.rss_max_kb"] = max(rsss)
            for suffix in ("rss_start_kb", "rss_end_kb", "rss_max_kb"):
                units[f"{target}.{suffix}"] = "kB"
    db.add_metrics(run_id, metrics, units)
    db.finish_run(run_id, benchenv.now_iso(), status="ok")
    _log(f"imported {rows} sample(s) as run {run_id} under label "
         f"'{args.label}'")
    _print_metrics(metrics, units)
    db.close()
    return 0


def cmd_export(args) -> int:
    """Write a campaign out as JSON, for keeping or for sharing.

    Args:
        args: the parsed command line.

    Returns:
        A process exit status.
    """
    db = _open_db(args.db)
    rows = db.runs(label=args.label, bench=args.bench)
    if not rows:
        _die(f"no run for label '{args.label}'")
    payload = []
    for row in rows:
        run = dict(row)
        run["params"] = json.loads(run.pop("params_json") or "{}")
        run["metrics"] = {k: {"value": v[0], "unit": v[1]}
                          for k, v in db.metrics(row["id"]).items()}
        if args.with_series:
            run["series"] = [dict(s) for s in db.series(row["id"])]
        payload.append(run)
    text = json.dumps(payload, indent=2, sort_keys=True)
    if args.out:
        with open(args.out, "w") as f:
            f.write(text + "\n")
        _log(f"wrote {len(payload)} run(s) to {args.out}")
    else:
        print(text)
    db.close()
    return 0


def cmd_list(_args) -> int:
    """List the available benchmarks.

    Args:
        _args: unused.

    Returns:
        A process exit status.
    """
    width = max(len(name) for name in (*BENCHES, *ROBOT_BENCHES))
    _log("./bench.py run <name>, for all of them:")
    for name in sorted(BENCHES):
        _log(f"  {name:<{width}}  {BENCHES[name]}")
    for name in sorted(ROBOT_BENCHES):
        robot_file, description = ROBOT_BENCHES[name]
        _log(f"  {name:<{width}}  {description}")
        _log(f"  {'':<{width}}  a robot test -- also runnable directly: "
             f"robot benchmarks/{robot_file}")
    _log("")
    _log("./bench.py probe measures daemons someone else started, on any")
    _log("machine -- a real installation, say. It files its window under the")
    _log("same 'load' bench, as variant 'probe', so a field measurement and a")
    _log("container one sit side by side in the store:")
    _log("  ./bench.py probe --help")
    return 0


def cmd_run(args) -> int:
    """Run one benchmark.

    Args:
        args: the parsed command line.

    Returns:
        A process exit status.
    """
    env = benchenv.describe()
    _guard_dirty(env, args.allow_dirty)
    label = _resolve_label(args.label, env)
    db = _open_db(args.db)
    try:
        if args.bench_name == "engine-config-load":
            return bench_config_load(args, db, env, label)
        if args.bench_name == "alloc":
            return bench_alloc(args, db, env, label)
        if args.bench_name in ROBOT_BENCHES:
            return bench_robot(args.bench_name, args, env, label)
        _die(f"unknown benchmark '{args.bench_name}' (try ./bench.py list)")
        raise AssertionError
    finally:
        db.close()


def build_parser() -> argparse.ArgumentParser:
    """Build the command line parser.

    Returns:
        The parser, with one subcommand per verb.
    """
    parser = argparse.ArgumentParser(
        description="Run the collect benchmarks and keep their results.",
        epilog=_MAIN_EPILOG,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--db", default=DEFAULT_DB,
                        help=f"result store (default {DEFAULT_DB})")
    sub = parser.add_subparsers(dest="command", required=True,
                                metavar="COMMAND")

    p_list = sub.add_parser("list", help="list the available benchmarks")
    p_list.set_defaults(func=cmd_list)

    p_run = sub.add_parser(
        "run", help="run a benchmark",
        description="Run one benchmark and file its results in the store. "
                    "Which options apply depends on the benchmark, and they are "
                    "grouped below accordingly.",
        epilog=_run_epilog(),
        formatter_class=argparse.RawDescriptionHelpFormatter)
    p_run.add_argument("bench_name", metavar="BENCH",
                       choices=[*BENCHES, *ROBOT_BENCHES],
                       help="which benchmark: " +
                            ", ".join(sorted([*BENCHES, *ROBOT_BENCHES])))

    g_common = p_run.add_argument_group("common to every benchmark")
    g_common.add_argument("--label",
                          help="names the campaign (default: the git branch)")
    g_common.add_argument("--allow-dirty", action="store_true",
                          help="measure even though the working tree is "
                               "modified, and say so in the store")
    g_common.add_argument("--engine",
                          help="centengine binary: the one to measure for "
                               "engine-config-load, the one to record in the "
                               "notes for alloc (default: the installed one)")

    g_cl = p_run.add_argument_group("engine-config-load only")
    g_alloc = p_run.add_argument_group("alloc only")
    g_robot = p_run.add_argument_group(
        "engine-startup, load and rrd-retention only (they are robot tests, "
        "and these are forwarded to robot)")
    g_cl.add_argument("--sizes", default="1000,10000,50000",
                      help="service counts to sweep")
    g_cl.add_argument("--services-by-host", type=int, default=20,
                      help="services per host (default 20)")
    g_cl.add_argument("--commands", type=int, default=50,
                      help="size of the check command pool")
    g_cl.add_argument("--repeat", type=int, default=3,
                      help="measurements per size (default 3)")
    g_cl.add_argument("--mode", choices=("verify", "test-scheduling", "both"),
                      default="verify",
                      help="-v, -s, or both (default verify)")
    g_cl.add_argument("--timezone", default=":Europe/Paris",
                      help="use_timezone value, needs tzdata")
    g_cl.add_argument("--workdir",
                      help="where the configurations are written")
    g_cl.add_argument("--keep", action="store_true",
                      help="keep the generated configurations")
    g_alloc.add_argument("--profile", default="EALLOC2",
                         choices=(*ALLOC_PROFILES, "all"),
                         help="which profile to measure (default EALLOC2, "
                         "the nominal active check one)")
    g_alloc.add_argument("--ready-timeout", type=float, default=900.0,
                         help="seconds granted to the test to warm "
                         "centengine up (default 900)")
    g_alloc.add_argument("--attach-timeout", type=float, default=300.0,
                         help="seconds granted to the gdb injection "
                         "(default 300)")
    g_alloc.add_argument("--done-timeout", type=float, default=5400.0,
                         help="seconds granted to the workload "
                         "(default 5400; EALLOC1 submits 100000 results)")
    g_alloc.add_argument("--flush-timeout", type=float, default=900.0,
                         help="seconds granted to heaptrack to flush its "
                         "trace once centengine has stopped (default 900)")
    g_alloc.add_argument("--teardown-timeout", type=float, default=600.0,
                         help="seconds granted to the test to tear down "
                         "(default 600)")
    g_robot.add_argument("--var", action="append", metavar="NAME:VALUE",
                         help="a robot "
                         "variable, repeatable (e.g. --var duration:1800)")
    g_robot.add_argument("--test",
                         help="run only that "
                         "test of the suite (e.g. BENCH_LOAD_PASSIVE)")
    p_run.set_defaults(func=cmd_run)

    p_probe = sub.add_parser(
        "probe", help="measure the running collect daemons over a window")
    p_probe.add_argument("--label",
                         help="names the campaign (default: the git branch)")
    p_probe.add_argument("--duration", type=int, default=600,
                         help="measurement window in seconds (default 600)")
    p_probe.add_argument("--warmup", type=int, default=120,
                         help="seconds ignored at the beginning (default 120); "
                              "0 if the daemons have been up for a while")
    p_probe.add_argument("--interval", type=int, default=60,
                         help="sampling period in seconds (default 60); does "
                              "not affect the cumulative totals")
    p_probe.add_argument("--engine-config",
                         default="/etc/centreon-engine/centengine.cfg",
                         help="passed to centenginestats for the check counters")
    p_probe.add_argument("--variant", default="probe",
                         help="what kind of window this is, e.g. active or "
                              "passive; part of what pairs two runs in compare")
    p_probe.add_argument("--param", action="append", metavar="KEY=VALUE",
                         help="describe the load being measured, repeatable "
                              "(e.g. --param hosts=50 --param services=1000); "
                              "also part of the pairing key")
    p_probe.add_argument("--notes", help="free text kept with the run")
    p_probe.set_defaults(func=cmd_probe)

    p_show = sub.add_parser("show", help="show what the store holds")
    p_show.add_argument("--label", help="restrict to one campaign")
    p_show.add_argument("--bench", help="restrict to one benchmark")
    p_show.add_argument("--run", type=int, help="detail one run by id")
    p_show.add_argument("--metrics",
                        help="comma separated metric names to keep")
    p_show.set_defaults(func=cmd_show)

    p_cmp = sub.add_parser("compare", help="compare two campaigns")
    p_cmp.add_argument("label_a", metavar="LABEL_A")
    p_cmp.add_argument("label_b", metavar="LABEL_B")
    p_cmp.add_argument("--bench", help="restrict to one benchmark")
    p_cmp.add_argument("--diff", action="store_true",
                       help="for alloc runs, also print the per-stack "
                            "difference between the two heaptrack traces")
    p_cmp.set_defaults(func=cmd_compare)

    p_imp = sub.add_parser("import-csv",
                           help="import a CSV written by the retired "
                                "bench-load.sh")
    p_imp.add_argument("--csv", required=True, help="the CSV file")
    p_imp.add_argument("--label", required=True,
                       help="campaign to file it under, e.g. 25.10")
    p_imp.add_argument("--host", help="where it was measured")
    p_imp.add_argument("--commit", help="commit it was measured on, if known")
    p_imp.set_defaults(func=cmd_import_csv)

    p_exp = sub.add_parser("export", help="write a campaign out as JSON")
    p_exp.add_argument("--label", required=True)
    p_exp.add_argument("--bench")
    p_exp.add_argument("--out", help="output file (default stdout)")
    p_exp.add_argument("--with-series", action="store_true",
                       help="include the indexed measurements, not only the "
                            "scalars")
    p_exp.set_defaults(func=cmd_export)

    return parser


def main(argv=None) -> int:
    """Parse the command line and run the requested verb.

    Args:
        argv (list, optional): arguments. Defaults to None, sys.argv.

    Returns:
        A process exit status.
    """
    # Die like any other command line tool when its reader goes away: without
    # this, a plain "./bench.py show | head" ends on a BrokenPipeError traceback
    # instead of just stopping.
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

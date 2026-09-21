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
"""The keywords a benchmark robot test needs and the test suite does not have.

Two of them, and both exist for the same reason: doing it in robot would make
robot itself part of what is measured.

Sustaining a passive load means writing thousands of lines into the Engine
command FIFO. A robot FOR loop calling Ctn Process Service Check Result once per
result would spend more time in robot than in Engine, and would compete for the
CPU with the very daemons whose CPU is being measured.

Reading a result back out of the store is the other one: the measurement is
written by bench.py, in another process, and the test has to be able to say
whether the load it built was real.
"""

import datetime
import os
import re
import sys
import time
from typing import Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import benchdb  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_DB = os.path.join(HERE, "results", "bench.db")
VAR_ROOT = "/tmp/var"


def _command_file(config: str = "config0") -> str:
    """Return the external command FIFO of an Engine instance.

    Args:
        config (str, optional): instance directory name. Defaults to "config0".

    Returns:
        The path of the FIFO.
    """
    return f"{VAR_ROOT}/lib/centreon-engine/{config}/rw/centengine.cmd"


def _service_of(host: int, index: int, services_by_host: int) -> int:
    """Return the id of a service, as the generated configuration numbers them.

    Service ids are global and sequential in the configuration the test suite
    generates: host_1 owns service_1 to service_20, host_2 service_21 to 40, and
    so on. Getting this wrong would submit results for services that do not
    exist, which Engine drops silently -- a load that measures nothing.

    Args:
        host (int): host number, from 1.
        index (int): service number within that host, from 1.
        services_by_host (int): how many services each host owns.

    Returns:
        The global service number.
    """
    return (host - 1) * services_by_host + index


def ctn_bench_submit_passive_results(count: int, hosts: int = 50,
                                     services_by_host: int = 20,
                                     state: int = 0,
                                     output: str = "BENCH - passive result",
                                     config: str = "config0") -> int:
    """Submit passive check results, spread over every service.

    Args:
        count (int): how many results to submit.
        hosts (int, optional): number of hosts in the configuration. Defaults to 50.
        services_by_host (int, optional): services per host. Defaults to 20.
        state (int, optional): the state to submit. Defaults to 0, OK.
        output (str, optional): the check output. Defaults to a realistic one --
            an output of fifteen bytes or less allocates nothing at all thanks to
            the small string optimization, and would understate the cost.
        config (str, optional): Engine instance. Defaults to "config0".

    Returns:
        How many results were written.
    """
    total_services = hosts * services_by_host
    if total_services <= 0 or count <= 0:
        return 0
    now = int(time.time())
    written = 0
    # One open for the whole batch: the FIFO is opened for writing, which Engine
    # sees as a single burst rather than as thousands of separate writers.
    with open(_command_file(config), "w") as f:
        for i in range(count):
            slot = i % total_services
            host = slot // services_by_host + 1
            service = _service_of(host, slot % services_by_host + 1,
                                  services_by_host)
            f.write(f"[{now}] PROCESS_SERVICE_CHECK_RESULT;host_{host};"
                    f"service_{service};{state};{output} {i}\n")
            written += 1
    return written


def ctn_bench_sustain_passive_load(rate: int, duration: int, hosts: int = 50,
                                   services_by_host: int = 20,
                                   state: int = 0,
                                   output: str = "BENCH - passive result",
                                   config: str = "config0",
                                   period: float = 1.0) -> int:
    """Keep submitting passive results at a steady rate, for a duration.

    A steady rate is the point: a burst would measure how fast Engine drains a
    backlog, which is not what a poller does. The submission cost itself is kept
    out of the rate by measuring how long each batch took and sleeping the rest
    of the period.

    Args:
        rate (int): results per second.
        duration (int): for how many seconds.
        hosts (int, optional): number of hosts. Defaults to 50.
        services_by_host (int, optional): services per host. Defaults to 20.
        state (int, optional): the state to submit. Defaults to 0.
        output (str, optional): the check output. Defaults to a realistic one.
        config (str, optional): Engine instance. Defaults to "config0".
        period (float, optional): seconds between two batches. Defaults to 1.

    Returns:
        How many results were submitted in total.
    """
    per_batch = max(1, int(rate * period))
    deadline = time.monotonic() + duration
    total = 0
    while time.monotonic() < deadline:
        batch_started = time.monotonic()
        total += ctn_bench_submit_passive_results(
            per_batch, hosts, services_by_host, state, output, config)
        left = period - (time.monotonic() - batch_started)
        if left > 0:
            time.sleep(min(left, max(0.0, deadline - time.monotonic())))
    return total


def ctn_bench_bam_kpi_services(nb_ba: int, kpi_per_ba: int, hosts: int = 50,
                               services_by_host: int = 20) -> list:
    """Pick the services of each BA of the steady-state BAM benchmark.

    The BAs share nothing: BA 1 takes the first kpi_per_ba services of the
    configuration, BA 2 the next ones, and so on, so that every passive result
    submitted to those services reaches exactly one KPI. The services are named
    as the generated configuration names them.

    Args:
        nb_ba (int): how many BAs.
        kpi_per_ba (int): service KPIs per BA.
        hosts (int, optional): hosts in the configuration. Defaults to 50.
        services_by_host (int, optional): services per host. Defaults to 20.

    Returns:
        A list of nb_ba lists of (host name, service description) couples.

    Raises:
        ValueError: when the configuration has fewer services than the BAs need.
    """
    nb_ba = int(nb_ba)
    kpi_per_ba = int(kpi_per_ba)
    hosts = int(hosts)
    services_by_host = int(services_by_host)
    if nb_ba * kpi_per_ba > hosts * services_by_host:
        raise ValueError(f"{nb_ba} BAs of {kpi_per_ba} KPIs need "
                         f"{nb_ba * kpi_per_ba} services, the configuration "
                         f"has {hosts * services_by_host}")
    result = []
    slot = 0
    for _ in range(nb_ba):
        services = []
        for _ in range(kpi_per_ba):
            host = slot // services_by_host + 1
            service = _service_of(host, slot % services_by_host + 1,
                                  services_by_host)
            services.append((f"host_{host}", f"service_{service}"))
            slot += 1
        result.append(services)
    return result


def ctn_bench_last_run_metrics(label: str, bench: str = "load",
                               variant: str = "", db_path: str = "") -> dict:
    """Read back the metrics of the most recent matching run.

    Args:
        label (str): the campaign.
        bench (str, optional): the benchmark. Defaults to "load".
        variant (str, optional): keep only that variant. Defaults to "", any.
        db_path (str, optional): the store. Defaults to "", results/bench.db.

    Returns:
        Metric name to value. Empty when there is no such run, which a test
        should treat as a failure rather than as zero.
    """
    with benchdb.BenchDB(db_path or DEFAULT_DB) as db:
        for row in db.runs(label=label, bench=bench):
            if variant and row["variant"] != variant:
                continue
            return {name: value
                    for name, (value, _unit) in db.metrics(row["id"]).items()}
    return {}


def ctn_bench_add_metric(label: str, name: str, value: float,
                         unit: str = "", bench: str = "load",
                         variant: str = "", db_path: str = "") -> bool:
    """Add a metric to the most recent matching run.

    What the probe cannot know goes through here. The passive profile is the
    case: centenginestats only counts *active* checks, so in a passive window it
    reports none and any cost per check derived from it is meaningless. The
    number of results actually submitted is known by the test, and only by the
    test, so the test is what files it.

    Args:
        label (str): the campaign.
        name (str): metric name.
        value (float): its value.
        unit (str, optional): its unit. Defaults to "".
        bench (str, optional): the benchmark. Defaults to "load".
        variant (str, optional): keep only that variant. Defaults to "", any.
        db_path (str, optional): the store. Defaults to "", results/bench.db.

    Returns:
        True if a run was found and updated.
    """
    with benchdb.BenchDB(db_path or DEFAULT_DB) as db:
        for row in db.runs(label=label, bench=bench):
            if variant and row["variant"] != variant:
                continue
            db.add_metric(row["id"], name, float(value), unit or None)
            db.commit()
            return True
    return False


def _parse_log_timestamp(line: str):
    """Read the date of a centengine log line.

    Args:
        line (str): a log line, starting with "[2026-08-20T14:58:47.531+02:00]".

    Returns:
        A datetime, or None if the line does not start with one.
    """
    if not line.startswith("["):
        return None
    end = line.find("]")
    if end < 0:
        return None
    try:
        return datetime.datetime.fromisoformat(line[1:end])
    except ValueError:
        return None


def ctn_bench_startup_timings(log_path: str) -> dict:
    """Read the startup phases Engine logged out of its log file.

    Engine emits one "Startup timing: <phase> = <n> ms" line per phase, at info,
    once per startup. They are the only way to attribute a slow start to a phase:
    --verify-config runs parse, expand, resolve and apply as one block, so
    measuring from the outside gives a total and nothing else.

    The last value of each phase wins, so that a log kept across a restart
    describes the most recent startup rather than a mix of two.

    Three prefixes are collected, and keeping them apart matters. A poller whose
    configuration is owned by Broker starts on whatever it has locally -- which
    may be nothing -- and receives the real configuration afterwards, as a diff
    carrying the full state. That application is logged as "Reload timing", not
    "Startup timing", so a cold centralized start shows near-zero startup phases
    and all of its actual cost under reload.*. Reading only the startup ones
    would report such a poller as starting in five milliseconds.

    Args:
        log_path (str): the centengine log file.

    Returns:
        A dict of phase name to milliseconds -- bare for the startup phases,
        prefixed with "reload." or "diff." for the other two -- plus
        "log_wall_ms": the time from the first log line to the one announcing the
        event loop, which is what an operator waiting for the poller actually
        experiences. Empty if the log carries no timing line at all: an Engine
        built without the instrumentation, which a test should treat as a failure.
    """
    pattern = re.compile(r"(Startup|Reload|Diff) timing: (\S+) = (\d+) ms")
    prefix_of = {"Startup": "", "Reload": "reload.", "Diff": "diff."}
    timings = {}
    first_ts = None
    loop_ts = None
    with open(log_path, "r", errors="replace") as f:
        for line in f:
            ts = _parse_log_timestamp(line)
            if ts is not None and first_ts is None:
                first_ts = ts
            match = pattern.search(line)
            if match:
                kind, phase, value = match.groups()
                timings[f"{prefix_of[kind]}{phase}"] = float(value)
            elif "Event loop start at" in line and ts is not None:
                loop_ts = ts
    if first_ts is not None and loop_ts is not None:
        timings["log_wall_ms"] = (loop_ts - first_ts).total_seconds() * 1000
    return timings


def ctn_bench_record_run(label: str, bench: str, variant: str,
                         metrics: dict, params: Optional[dict] = None,
                         notes: str = "", unit: str = "",
                         db_path: str = "") -> int:
    """File a complete measurement in the store, in one call.

    For a benchmark whose figures come from the product itself -- the startup
    phases Engine logs -- rather than from a probe watching it from outside.

    Args:
        label (str): the campaign.
        bench (str): the benchmark name.
        variant (str): which flavour of it.
        metrics (dict): metric name to value; values may be strings, robot
            dictionaries carry them as such.
        params (dict, optional): what parameterises the point, and what pairs two
            runs when comparing campaigns. Defaults to None.
        notes (str, optional): free text. Defaults to "".
        unit (str, optional): unit shared by every metric of the call, for
            display. Defaults to "", none -- names ending in "_ms" are taken as
            milliseconds whatever this says.
        db_path (str, optional): the store. Defaults to "", results/bench.db.

    Returns:
        The run id.
    """
    sys.path.insert(0, HERE)
    import benchenv

    cleaned = {}
    for name, value in (params or {}).items():
        try:
            cleaned[name] = int(value)
        except (TypeError, ValueError):
            cleaned[name] = value

    with benchdb.BenchDB(db_path or DEFAULT_DB) as db:
        run_id = db.start_run(label=label, bench=bench, variant=variant,
                              started_at=benchenv.now_iso(), params=cleaned,
                              env=benchenv.describe(), notes=notes or None)
        db.add_metrics(run_id, {name: float(value)
                                for name, value in metrics.items()},
                       {name: "ms" if name.endswith("_ms") else unit
                        for name in metrics})
        db.finish_run(run_id, benchenv.now_iso(), status="ok")
        return run_id


def ctn_bench_git_branch() -> str:
    """Return the current git branch, which is the default campaign name.

    Returns:
        The branch name, or "unnamed" outside of a working tree. The test needs
        it because it has to know, before bench.py runs, under which label the
        result will be filed in order to read it back afterwards.
    """
    sys.path.insert(0, HERE)
    import benchenv
    return benchenv.git_info().get("git_branch") or "unnamed"


# The steps reader_v2::read() announces, in the order it announces them. Each
# line is logged *before* the step it names, so the duration of a step is the
# gap between its own line and the next one -- which is why the list has to stay
# ordered, and why the closing line is part of it.
BAM_LOAD_STEPS = (
    ("dimensions", "loading dimensions."),
    ("bas", "loading BAs."),
    ("kpis", "loading KPIs."),
    ("boolexps", "loading boolean expressions."),
    ("mapping", "loading mapping hosts <-> services."),
    (None, "bam configuration loaded."),
)


def ctn_bench_bam_load_timings(log_path: str) -> dict:
    """Read how long each step of the BAM configuration load took.

    bam::configuration::reader_v2::read() already announces every step at info,
    which is the whole instrumentation this benchmark needs: the gap between
    "loading mapping hosts <-> services." and "bam configuration loaded."
    isolates the one query that reads the entire service table, separated from
    the four other steps. Measuring cbd from the outside would give a startup
    duration and nothing else.

    Only the *last complete* sequence is returned. cbd runs read() again on
    every configuration reload, and a log that spans two of them would otherwise
    mix a step of one with a step of the next. A sequence that started but never
    reached its closing line is dropped for the same reason.

    Args:
        log_path (str): the central broker log file.

    Returns:
        A dict of "<step>_ms" to milliseconds, one per step, plus "total_ms",
        the whole of read(). Empty if the log carries no complete sequence --
        which means either that the bam logger is not at info, or that cbd never
        loaded a BAM configuration at all. A test should treat that as a failure
        rather than as a zero.
    """
    opening = BAM_LOAD_STEPS[0][1]
    closing = BAM_LOAD_STEPS[-1][1]
    last = {}
    current = None
    with open(log_path, "r", errors="replace") as f:
        for line in f:
            ts = _parse_log_timestamp(line)
            if ts is None:
                continue
            if opening in line:
                # A sequence that restarts before closing was a reload racing a
                # failed load; the partial one is worth nothing, drop it.
                current = {opening: ts}
                continue
            if current is None:
                continue
            for _, message in BAM_LOAD_STEPS[1:]:
                if message in line:
                    current[message] = ts
                    break
            if closing in line:
                last = current
                current = None
    if not last or len(last) != len(BAM_LOAD_STEPS):
        return {}

    timings = {}
    for (name, message), (_, following) in zip(BAM_LOAD_STEPS,
                                               BAM_LOAD_STEPS[1:]):
        timings[f"{name}_ms"] = (last[following]
                                 - last[message]).total_seconds() * 1000
    timings["total_ms"] = (last[closing]
                           - last[opening]).total_seconds() * 1000
    return timings


BAM_REBUILD_OPENING = "BAM-BI: will now rebuild the event durations"
BAM_REBUILD_CLOSING = "BAM-BI: event durations rebuild finished"
# "BAM-BI: 3 open BA events and 12 open KPI events loaded in 7 ms"
BAM_BI_CACHE_LOAD = re.compile(
    r"BAM-BI: (\d+) open BA events and (\d+) open KPI events loaded in (\d+) ms")
# A cbd from before that line: the load is framed by two trace lines, the
# second being the first thing the constructor does once the caches are in.
BAM_BI_CACHE_LOAD_OPENING = "reporting stream _load_kpi_ba_events"
BAM_BI_CACHE_LOAD_CLOSING = "reporting stream _close_inconsistent events (type BA"
# "BAM-BI: availabilities of 83 days written in 412 ms", and for a cbd from
# before that line, the two debug lines that frame the same work.
BAM_AVAIL = re.compile(r"BAM-BI: availabilities of (\d+) days written in (\d+) ms")
BAM_AVAIL_OPENING = "BAM-BI: availability thread writing availabilities from"
BAM_AVAIL_CLOSING = "BAM-BI: database closed"


def ctn_bench_bam_rebuild_timings(log_path: str) -> dict:
    """Read how long the BI rebuild of the event durations took.

    reporting_stream::_process_rebuild announces, at info, the moment it starts
    recomputing the durations and the moment it is done with them. Everything
    between the two is the loop this benchmark is about: one duration computed
    and written per (closed BA event, reporting period). What follows -- the
    availabilities -- runs in its own thread and is not measured here.

    The last complete pair wins, for the same reason as in
    ctn_bench_bam_load_timings.

    Args:
        log_path (str): the central broker log file.

    The startup of the reporting stream is read from the same log: the time it
    took to load its event caches, and how many open events it found. Until
    2026-09 the caches were the whole of the two event tables; a history that
    grows for years was read at every start. Filed as "bi_cache_load_ms",
    "bi_open_ba_events" and "bi_open_kpi_events" when the line is there -- an
    older cbd does not write it, and the rebuild figure stands on its own.

    Returns:
        A dict with "rebuild_ms", plus the cache figures above when available.
        Empty if the log carries no complete rebuild pair, which a test should
        treat as a failure: either the bam logger is not at info, or no rebuild
        happened.
    """
    last = {}
    cache = {}
    opening = None
    load_opening = None
    avail_opening = None
    with open(log_path, "r", errors="replace") as f:
        for line in f:
            ts = _parse_log_timestamp(line)
            if ts is None:
                continue
            if BAM_REBUILD_OPENING in line:
                opening = ts
            elif BAM_REBUILD_CLOSING in line and opening is not None:
                last = {"rebuild_ms": (ts - opening).total_seconds() * 1000}
                opening = None
            elif BAM_BI_CACHE_LOAD_OPENING in line:
                load_opening = ts
            elif BAM_BI_CACHE_LOAD_CLOSING in line and load_opening is not None:
                # Only a fallback, for a cbd that does not write the info line:
                # the bam logger has to be at trace for these two to be there.
                cache.setdefault("bi_cache_load_ms",
                                 (ts - load_opening).total_seconds() * 1000)
                load_opening = None
            elif BAM_AVAIL_OPENING in line:
                avail_opening = ts
            elif BAM_AVAIL_CLOSING in line and avail_opening is not None:
                # Same fallback for the availabilities, at debug.
                cache["availability_ms"] = (ts - avail_opening).total_seconds() * 1000
                avail_opening = None
            elif BAM_AVAIL.search(line):
                match = BAM_AVAIL.search(line)
                cache["availability_days"] = float(match.group(1))
                cache["availability_ms"] = float(match.group(2))
            else:
                match = BAM_BI_CACHE_LOAD.search(line)
                if match:
                    cache = {"bi_open_ba_events": float(match.group(1)),
                             "bi_open_kpi_events": float(match.group(2)),
                             "bi_cache_load_ms": float(match.group(3))}
    if last:
        last.update(cache)
    return last


def ctn_bench_process_rss_kb(pid) -> int:
    """Return the resident set size of a process, in kilobytes.

    The map reader_v2 fills is the other half of what the "load only the useful
    services" change is about: a platform-sized mapping costs tens of megabytes
    that are never read. A duration alone would miss it.

    Args:
        pid: the process id, as robot hands it over (a string or an int).

    Returns:
        The RSS in kB, or 0 if the process is already gone.
    """
    try:
        with open(f"/proc/{int(pid)}/status", "r") as f:
            for line in f:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1])
    except (OSError, ValueError):
        return 0
    return 0

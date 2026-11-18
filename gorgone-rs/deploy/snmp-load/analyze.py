#!/usr/bin/env python3
"""Summary of one run directory (run.py): statuses.csv of the sink and
metrics.csv of the cgroup sampler, over the steady window (after the first
WARMUP seconds of the engine).

  analyze.py RUN_DIR SERVICES INTERVAL_S
"""

import collections
import csv
import sys
from pathlib import Path

WARMUP = 60


def pct(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    values = sorted(values)
    return values[round((len(values) - 1) * p)]


def main() -> None:
    run, services, interval = Path(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    with open(run / "metrics.csv") as f:
        metrics = [{k: float(v) for k, v in row.items()} for row in csv.DictReader(f)]
    with open(run / "statuses.csv") as f:
        rows = list(csv.reader(f))
    t0 = metrics[0]["t"]
    start = next(m for m in metrics if m["t"] - t0 >= WARMUP)
    end = metrics[-1]
    wall = end["t"] - start["t"]

    begin, stop = start["epoch"], end["epoch"]
    window = [r for r in rows if begin <= float(r[0]) < stop]
    latency = [float(r[4]) for r in window]
    execution = [float(r[5]) for r in window]
    states = collections.Counter(int(r[6]) for r in window)
    unknown = collections.Counter(r[7][:50] for r in window if r[6] == "3")

    # Gap between two checks of one service: the interval when on time.
    last = {}
    gaps = []
    for r in sorted(rows, key=lambda r: int(r[3])):
        key = (r[1], r[2])
        if key in last:
            gaps.append(int(r[3]) - last[key])
        last[key] = int(r[3])
    checked = len({(r[1], r[2]) for r in rows})

    cores = (end["engine_cpu_usec"] - start["engine_cpu_usec"]) / 1e6 / wall
    agents = (end["agents_cpu_usec"] - start["agents_cpu_usec"]) / 1e6 / wall
    sink = (end["sink_cpu_usec"] - start["sink_cpu_usec"]) / 1e6 / wall
    rate = len(window) / wall
    expected = services / interval
    mib = 1024 * 1024
    print(f"steady window {wall:.0f}s (after {WARMUP}s), run {end['t'] - t0:.0f}s")
    print(f"checks/s       expected {expected:.1f}  achieved {rate:.1f} ({rate / expected:.0%})")
    print(f"engine cgroup  {cores:.2f} cores, {cores / rate * 1000 if rate else 0:.1f} ms CPU/check")
    print(f"memory         current {end['engine_mem_current'] / mib:.0f} MiB, "
          f"peak {end['engine_mem_peak'] / mib:.0f} MiB, pids max "
          f"{max(m['engine_pids'] for m in metrics):.0f}")
    print(f"latency s      p50 {pct(latency, .5):.2f}  p95 {pct(latency, .95):.2f}  "
          f"max {pct(latency, 1):.2f}  >1s {sum(v > 1 for v in latency) / max(len(latency), 1):.1%}"
          f"  >10s {sum(v > 10 for v in latency) / max(len(latency), 1):.1%}")
    print(f"execution s    p50 {pct(execution, .5):.3f}  p95 {pct(execution, .95):.3f}  "
          f"max {pct(execution, 1):.3f}")
    print(f"check gap s    p50 {pct(gaps, .5):.0f}  p95 {pct(gaps, .95):.0f}  "
          f"max {pct(gaps, 1):.0f}  (interval {interval}); "
          f"services checked {checked}/{services}")
    print(f"states         ok {states[0]} warn {states[1]} crit {states[2]} "
          f"unknown {states[3]} ({states[3] / max(len(window), 1):.1%})")
    for output, count in unknown.most_common(3):
        print(f"  unknown x{count}: {output}")
    print(f"snmp agents    {agents:.2f} cores (all agents), sink {sink:.3f} cores")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""One load run (README.md): starts the SNMP agents and the BBDO sink,
writes the engine configuration, runs the engine for DURATION seconds while
sampling the cgroups of the containers every 10 s, stops the engine and the
sink, then prints the analysis (analyze.py).

  run.py LABEL PLUGIN MODE SERVICES INTERVAL_S DURATION_S [MAX_CONCURRENT_CHECKS]

Environment: PLUGINS_DIR (centreon-plugins checkout, for the build),
AGENTS (20). Results in results/LABEL/. The agents stay up between runs;
`docker compose -f compose.yaml --profile engine down` removes everything.
"""

import os
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parent
COMPOSE = ["docker", "compose", "-f", str(HERE / "compose.yaml")]


def sh(*args: str, env: dict | None = None) -> str:
    return subprocess.run(args, check=True, capture_output=True, text=True, env=env).stdout


def cgroup(container_id: str) -> Path:
    return Path(f"/sys/fs/cgroup/system.slice/docker-{container_id}.scope")


def read_cpu_usec(container_id: str) -> int:
    for line in (cgroup(container_id) / "cpu.stat").read_text().splitlines():
        if line.startswith("usage_usec "):
            return int(line.split()[1])
    return 0


def read_int(container_id: str, name: str) -> int:
    try:
        return int((cgroup(container_id) / name).read_text())
    except (OSError, ValueError):
        return 0


def ids(env: dict, service: str) -> list[str]:
    return sh(*COMPOSE, "ps", "-q", "--no-trunc", service, env=env).split()


def main() -> None:
    label, plugin, mode, services, interval, duration, *rest = sys.argv[1:]
    max_checks = rest[0] if rest else "256"
    out = HERE / "results" / label
    out.mkdir(parents=True, exist_ok=True)
    os.chmod(out, 0o777)
    env = dict(os.environ)
    env.update(
        RESULTS_DIR=str(out),
        CONF_DIR=str(out / "conf"),
        MAX_CONCURRENT_CHECKS=max_checks,
        UID=str(os.getuid()),
    )
    sh(*COMPOSE, "up", "-d", "--no-build", "snmpd", env=env)
    sh(*COMPOSE, "up", "-d", "--no-build", "--force-recreate", "sink", env=env)
    agents = ids(env, "snmpd")
    # net-snmp answers hrProcessorLoad only once it holds one minute of CPU
    # samples: the cpu checks of a younger agent are UNKNOWN.
    started_at = sh("docker", "inspect", "-f", "{{.State.StartedAt}}", agents[0]).strip()
    age = time.time() - datetime.fromisoformat(started_at).timestamp()
    if age < 75:
        print(f"waiting {75 - age:.0f}s for the agents' CPU history")
        time.sleep(75 - age)
    addresses = [
        sh("docker", "inspect", "-f",
           "{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}", agent).strip()
        for agent in agents
    ]
    print(sh(sys.executable, "-I", str(HERE / "gen-config.py"), str(out / "conf"),
             plugin, services, interval, mode, *addresses), end="")
    sink = ids(env, "sink")[0]
    sh(*COMPOSE, "--profile", "engine", "up", "-d", "--no-build", "--force-recreate",
       "--no-deps", "engine", env=env)
    engine = ids(env, "engine")[0]
    print(f"run {label}: max_concurrent_checks={max_checks}, {duration}s")

    started = time.time()
    with open(out / "metrics.csv", "w") as metrics:
        metrics.write("epoch,t,engine_cpu_usec,engine_mem_current,engine_mem_peak,"
                      "engine_pids,agents_cpu_usec,sink_cpu_usec\n")
        while True:
            now = time.time()
            metrics.write(
                f"{now:.1f},{now - started:.1f},{read_cpu_usec(engine)},"
                f"{read_int(engine, 'memory.current')},{read_int(engine, 'memory.peak')},"
                f"{read_int(engine, 'pids.current')},"
                f"{sum(read_cpu_usec(agent) for agent in agents)},{read_cpu_usec(sink)}\n"
            )
            metrics.flush()
            if now - started >= int(duration):
                break
            time.sleep(10 - (time.time() - started) % 10)

    (out / "engine.log").write_text(
        subprocess.run([*COMPOSE, "logs", "--no-color", "engine"], env=env,
                       capture_output=True, text=True).stdout)
    sh(*COMPOSE, "--profile", "engine", "rm", "-sf", "engine", env=env)
    (out / "sink.log").write_text(sh(*COMPOSE, "logs", "--no-color", "sink", env=env))
    sh(*COMPOSE, "stop", "sink", env=env)
    print(sh(sys.executable, "-I", str(HERE / "analyze.py"), str(out), services, interval),
          end="")


if __name__ == "__main__":
    main()

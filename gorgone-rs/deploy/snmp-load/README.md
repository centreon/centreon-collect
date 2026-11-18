# SNMP load test of centreon-engine

What 10 000 SNMP services cost with centreon-engine (Rust), with the Rust
SNMP plugin of centreon-plugins and with the equivalent packaged Perl plugin.
The earlier test (`crates/centreon-engine/examples/load.rs`) ran `/bin/true`
and measured only the scheduler itself.

## Setup

| Part        | What                                                                                                                                                                                                                                          |
| ----------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Engine      | `centreon-engine` of this branch (`--features scheduler`), Engine files input (`gen-config.py`), Debian trixie-slim image, `max_concurrent_checks` 256 (default) unless stated                                                                |
| Rust plugin | `centreon-plugin-rust-snmp` 0.1.0 built from centreon-plugins `develop` (`rust-plugins/`, commit 08b6800, 2026-11-18) in `rust:1.97-alpine`, `--target x86_64-unknown-linux-musl`, collections of `rs-collections/applications-protocol-snmp` |
| Perl plugin | packaged `centreon-plugin-operatingsystems-linux-snmp` 20261000-1+deb13u1 from apt-plugins-stable trixie (as on a production Debian poller), perl 5.40.1, `libsnmp-perl` 5.9.4                                                                |
| SNMP agents | 20 containers of net-snmp 5.9.4 `snmpd` (Alpine 3.22), community `public`, HOST-RESOURCES-MIB of the container (the host's 8 CPUs and the container mounts)                                                                                   |
| Results     | `bbdo_sink` (`crates/centreon-engine/examples/bbdo_sink.rs`) in its own container: answers the BBDO handshake, acknowledges, writes one CSV line per service status (`latency`, `execution_time`, `state`, output)                            |
| Topology    | 10 000 services, 20 per host, 500 hosts, host _n_ on agent _n_ mod 20; no host checks; first checks spread over the whole interval (`max_service_check_spread` = interval)                                                                    |
| Machine     | WSL2 on a laptop i7-1255U (8 vCPU exposed: 4 cores × 2 threads; 15 W part, turbo varies with load), 19 GiB; other desktop workloads ran at the same time; scenarios one at a time                                                             |

Commands (`$HOSTADDRESS$` is the agent address):

```
# Rust, cpu (hrProcessorLoad walk) and storage (hrStorageTable walk)
centreon-plugin-rust-snmp -H $HOSTADDRESS$ -v 2c -c public \
  -j /usr/share/centreon/rs-collections/applications-protocol-snmp/cpu.json --warning-avg 80 --critical-avg 90
centreon-plugin-rust-snmp -H $HOSTADDRESS$ -v 2c -c public \
  -j /usr/share/centreon/rs-collections/applications-protocol-snmp/storage.json --warning-prct 80 --critical-prct 90
# Perl, same MIB tables
centreon_linux_snmp.pl --plugin=os::linux::snmp::plugin --mode=cpu --hostname=$HOSTADDRESS$ \
  --snmp-version=2c --snmp-community=public --warning-average=80 --critical-average=90
centreon_linux_snmp.pl --plugin=os::linux::snmp::plugin --mode=storage --hostname=$HOSTADDRESS$ \
  --snmp-version=2c --snmp-community=public --warning-usage=80 --critical-usage=90
```

## Running it

From this directory, with a centreon-plugins checkout (`git clone --depth 1
--branch develop https://github.com/centreon/centreon-plugins.git <dir>`):

```bash
export PLUGINS_DIR=<dir> AGENTS=20
docker compose -f compose.yaml --profile engine build
# LABEL PLUGIN MODE SERVICES INTERVAL_S DURATION_S [MAX_CONCURRENT_CHECKS]
python3 -I run.py rust-cpu-5m  rust cpu 10000 300 480
python3 -I run.py rust-cpu-1m  rust cpu 10000 60 300
python3 -I run.py perl-cpu-5m  perl cpu 10000 300 480
python3 -I run.py perl-cpu-1m  perl cpu 10000 60 300
python3 -I run.py perl-cpu-1m-c16 perl cpu 10000 60 300 16
python3 -I run.py rust-storage-1m rust storage 10000 60 300
# Cost of one execution of each plugin, alone (200 sequential runs)
docker compose -f compose.yaml up -d --no-build snmpd   # wait 75 s, see below
docker run --rm --network snmp-load_default --entrypoint sh -v "$PWD/plugin-cost.sh:/plugin-cost.sh:ro" \
  snmp-load-engine /plugin-cost.sh "$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' snmp-load-snmpd-1)" 200
docker compose -f compose.yaml --profile engine down
```

`run.py` starts the agents and the sink, writes the configuration in
`results/LABEL/conf`, starts the engine, samples every 10 s the cgroups of
the engine container (engine and plugins: `cpu.stat` `usage_usec`,
`memory.current`, `memory.peak`, `pids.current`), of the agents and of the
sink (`results/LABEL/metrics.csv`), stops the engine and the sink, and runs
`analyze.py` on the steady window (after the first 60 s). Latency is the
BBDO `latency` of each status (how late the check started against its
planned time, ready-queue wait included); the check gap is the time between
two `last_check` of one service.

net-snmp answers `hrProcessorLoad` only once it holds one minute of CPU
samples: `run.py` waits until the agents are 75 s old.

## Results (2026-11-18)

### Per plugin, alone (200 sequential runs, CPU of the plugin's cgroup)

| Plugin                             | CPU per run | Wall per run | Max RSS  |
| ---------------------------------- | ----------- | ------------ | -------- |
| Rust cpu                           | 1.6 ms      | 3.0 ms       | 2.4 MiB  |
| Rust storage                       | 3.2 ms      | 14.7 ms      | 2.3 MiB  |
| Perl cpu                           | 74.1 ms     | 75.8 ms      | 28.0 MiB |
| Perl storage                       | 82.7 ms     | 84.4 ms      | 28.9 MiB |
| `/bin/true` (fork + exec baseline) | 0.4 ms      | 0.4 ms       | 1.2 MiB  |

Most of the Perl time is the interpreter start and the compilation of the
plugin and its modules, paid on every check; the SNMP exchange itself is a
few milliseconds, as the Rust numbers show.

### Under the engine, 10 000 services

Engine container = engine + plugins. CPU in cores over the steady window;
memory at the end / peak.

| Scenario                        | Checks/s expected → achieved | CPU (cores) | CPU per check | Memory                    | Latency p50 / p95 / max    | Execution p50 / p95 / max | Check gap p50 | Failures  | Agents CPU |
| ------------------------------- | ---------------------------- | ----------- | ------------- | ------------------------- | -------------------------- | ------------------------- | ------------- | --------- | ---------- |
| 1. Rust cpu, 5 min              | 33.3 → 33.3                  | 0.07        | 2.0 ms        | 26 / 44 MiB               | 0 / 0 / 0.01 s             | 4 / 5 / 17 ms             | 300 s         | 0         | 0.01       |
| 2. Rust cpu, 1 min              | 166.7 → 166.7                | 0.23        | 1.4 ms        | 26 / 44 MiB               | 0 / 0 / 0.04 s             | 3 / 3 / 46 ms             | 60 s          | 0         | 0.03       |
| 3. Perl cpu, 5 min              | 33.3 → 33.3                  | 3.77        | 113 ms        | 62 / 133 MiB              | 0 / 0 / 0.01 s             | 112 / 153 / 305 ms        | 300 s         | 0         | 0.02       |
| 4. Perl cpu, 1 min, 256 at once | 166.7 → 34.9 (21 %)          | 7.39        | 212 ms        | 1265 / 2002 MiB, 260 pids | 144 / 216 / 218 s, growing | 7.0 / 11.2 / 19.6 s       | 282 s         | 0 UNKNOWN | 0.03       |
| 4b. Perl cpu, 1 min, 16 at once | 166.7 → 42.0 (25 %)          | 7.74        | 184 ms        | 207 / 247 MiB, 27 pids    | 132 / 176 / 178 s, growing | 0.39 / 0.52 / 0.77 s      | 235 s         | 0 UNKNOWN | 0.03       |
| 5. Rust storage, 1 min          | 166.7 → 88.8 (53 %)          | 0.46        | 5.1 ms        | 152 / 153 MiB, 267 pids   | 48 / 52 / 53 s, growing    | 2.85 / 3.13 / 3.76 s      | 112 s         | 0         | 0.26       |

Notes:

- Scenario 4 saturates the whole machine (7.4 of 8 vCPUs). The engine does
  not drop or time out checks (`service_check_timeout` 30 s, none reached):
  every due check waits in the ready queue, the latency grows by about
  0.75 s every second and each service is checked every 4 to 5 minutes
  instead of every minute. With 256 Perl processes at once the plugins take
  7 s each and the container holds 2 GiB (about 8 MiB per Perl process);
  with 16 at once the throughput is 20 % higher (less contention), each check
  takes 0.4 s and the memory falls to 250 MiB. The limit is the CPU, not
  `max_concurrent_checks`: the machine tops out at about 40 Perl checks/s.
  All results are CRITICAL there: the agents report the host's CPUs, which
  the Perl plugins themselves keep above the 90 % threshold.
- Per-check CPU under load is higher than alone (Perl 113 to 212 ms against
  74 ms): concurrency on a laptop part whose clock drops when all cores run,
  and hyper-threads sharing cores.
- Scenario 5 is limited by the SNMP agents, not by the engine (0.46 cores):
  a storage walk needs about 14 GetBulk round trips (max-repetitions 10, a
  new UDP socket each), and the agents together answer about 95 storage
  walks/s whatever their number (reproduced without the engine: 1 agent with
  20 clients 101 walks/s, 13 agents with 20 clients each 96 walks/s), with
  little CPU. All agents share one kernel here; real devices would not. The
  engine side cost (5.1 ms per check) is the useful number.

## Conclusions

- Where the cost is: with the Rust plugin, the engine plus plugin cost
  1.4 to 2 ms of CPU per check, of which the plugin process is most
  (fork, exec, one SNMP walk); the scheduler's own share is the 0.25 to
  0.4 ms of the `/bin/true` test. With the Perl plugin, 113 ms per check
  (74 ms alone), almost all of it Perl start and compilation.
- Rust plugin: one core runs about 500 to 700 cpu checks/s, i.e. 30 000 to
  40 000 services every minute or 150 000 to 200 000 every 5 minutes; memory
  does not matter (26 MiB for 10 000 services, one plugin in flight at a
  time on average).
- Perl plugin: one core runs about 9 checks/s, i.e. about 500 services every
  minute or 2 600 every 5 minutes; 10 000 services every 5 minutes take
  3.8 cores, every minute about 19 cores (more than this machine has).
  Memory is about 8 MiB per Perl process in flight, so `max_concurrent_checks`
  bounds it (256 at once: 2 GiB).
- Ratio: about 45 (alone) to 55 (scenario 3) times more CPU per check for
  Perl than for Rust on the same MIB table.

## Next comparison: the C++ Engine with centreon-connector-perl

The C++ Engine runs Perl plugins through `centreon_connector_perl`
(`connectors/perl/`, `doc/ARCHITECTURE.md`): one process per script holds an
embedded interpreter with the plugin compiled once, and forks a worker per
check, so the start and compilation cost (most of the 74 ms) is not paid per
check. That is the reference the Perl numbers above must be compared with
next; centreon-engine (Rust) runs every Perl plugin as a fresh process and
has no connector.

# OTLP integration tests

`otlp.robot` starts a Python OTLP `MetricsService` on an ephemeral loopback
port. Broker exports to this server; tests inspect the actual resource and
datapoint attributes. It uses the existing Engine/Broker configuration helpers,
with only an OTLP output: no database, RRD daemon or external collector is needed.

Run from `tests/`, with the usual Robot/Python dependencies installed:

```sh
./init-proto.sh
python3 -m robot --outputdir /tmp/otlp-results broker-engine/otlp.robot
```

Like the other suites, it starts and stops processes with the shared `Ctn Start
Broker`/`Ctn Start Engine` keywords, so it runs the installed `/usr/sbin/cbd`,
`/usr/sbin/centengine` and Broker modules from
`/usr/share/centreon/lib/centreon-broker`. Install binaries and modules built
from the same revision before running it.

The suite also regenerates the test configurations under `EtcRoot` and clears
Broker caches/logs under `VarRoot` (normally `/tmp/etc` and `/tmp/var`). Run it
separately from other suites using those directories/ports. The Engine scenarios
use BBDO 3.0.1 over TCP; the controlled event scenarios replace Broker's 5669
input with BBDO 3.1 over gRPC, with an actual Welcome handshake. The producer
deliberately uses a transport `source_id` different from the poller `instance_id`.

Coverage:

- Default identity, both host overrides, individual fallbacks, per-host isolation,
  whitespace, `service.version`, and exclusion of service macros/secrets.
- Runtime macro changes, configuration reload/removal (`is_sent` regression),
  Engine startup cleanup, Broker restart, and persisted cache without a replay.
- Destination-host ordering during migration, rejection of stale updates and
  deletions, unknown/deleted hosts, and acceptance of older protobuf events that
  omit `instance_id`, plus BBDO2 custom-variable configuration/status conversion.
- Agent host/OS resource attributes and opt-in IPv4/IPv6 link-local filtering.
- Metric mapping, scaling, live reload, and invalid-file fallback.

Each passive probe has a new value. Assertions wait for that exact host, metric
and value, then check its identity; they do not wait until a desired identity
eventually appears. A probe sent through the old poller also ensures its stale
macro updates have crossed the same input stream before the assertion.
The collector records every datapoint in the Broker log directory as
`otlp.jsonl`; on failure, `Ctn Save Logs If Failed` copies it with the process
output, Broker/Engine logs and configurations to `failed/<test name>/`.

The metadata tests inject `AgentHostInfo` at Broker's input; they do not run CMA
or test OS discovery. The legacy test transports BBDO2-layout payloads in gRPC's
raw buffer envelope to exercise Broker's binary decoder and conversion; it does
not require an old Engine binary.

The Python collector has a small independent gRPC test suite:

```sh
python3 -m unittest discover -s resources -p test_otlp.py
```

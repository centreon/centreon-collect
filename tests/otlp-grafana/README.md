# Watching the Broker OTLP output in Grafana

A throwaway stack that receives what the Broker `otlp` output exports and graphs it.

```
cbd (on the host) --OTLP/gRPC--> otel-collector:4317
                                       | remote write
                                       v
                                 prometheus:9090
                                       v
                                  grafana:3000
```

Broker runs on the host, not in the compose network, so the collector publishes 4317 on the
host and the broker endpoint stays `127.0.0.1:4317`.

## Run it

```bash
cd tests/otlp-grafana
docker compose up -d

cd ..
robot -v OTLP_PORT:4317 -v EXTERNAL_COLLECTOR:True -t BEOTLPSOAK .
```

Then open <http://localhost:3000> — anonymous admin, no login — and pick the **Centreon Broker
OTLP output** dashboard. It refreshes every 10s; the soak test never stops, so leave it running.

`EXTERNAL_COLLECTOR:True` tells the suite not to start its own Python collector, which would
otherwise fight for the port. All the assertions it keeps are read from the Broker side, so the
test still fails if the export stalls.

Teardown:

```bash
docker compose down -v
```

## What you get

| Panel | What it tells you |
|---|---|
| hosts / services reporting | how many distinct `host.name` and service descriptions made it through the global cache enrichment |
| perfdata points/s | throughput of the whole chain: engine check → muxer → otlp stream → collector → Prometheus |
| newest check age | Broker timestamps with `last_check`, so this sits around one check interval; if it climbs, the export stalled |
| centreon.metric | one series per (host, service, metric) |
| centreon.check.state / centreon.host.state | state timelines, colour mapped OK/WARNING/CRITICAL/UNKNOWN |
| value against its thresholds | the value with its warning and critical bounds dashed on top |
| current state per service | instant table with `centreon.host.id` / `centreon.service.id` |

## Names and labels

For the full chain — which BBDO field becomes which OTLP attribute, where it is attached in the
protobuf, and what the collector renames it to — see
[broker/doc/otlp_attribute_flow.md](../../broker/doc/otlp_attribute_flow.md).

The collector translates OTLP to Prometheus, so dots become underscores.
`translation_strategy: UnderscoreEscapingWithoutSuffixes` in
[otel-collector.yml](otel-collector.yml) keeps units from being appended, so the names stay
predictable (on collectors older than 0.146 that setting is `add_metric_suffixes: false`):

| OTLP metric | Prometheus |
|---|---|
| `centreon.metric` (or its semconv name) | `centreon_metric` |
| `centreon.metric.threshold` | `centreon_metric_threshold` |
| `centreon.metric.bound` | `centreon_metric_bound` |
| `centreon.check.state` | `centreon_check_state` |
| `centreon.check.state_type` | `centreon_check_state_type` |
| `centreon.host.state` | `centreon_host_state` |
| `centreon.host.state_type` | `centreon_host_state_type` |

State values are `0 OK, 1 WARNING, 2 CRITICAL, 3 UNKNOWN, 4 PENDING` for services and
`0 UP, 1 DOWN, 2 UNREACHABLE, 4 PENDING` for hosts — 4 is what engine reports for a check that has
never run, and 3 is unused for hosts. `*_state_type` is `1` for hard and `0` for soft; it is a
metric rather than an attribute of the state so that a soft/hard transition does not end the state
series and start a new one.

`resource_to_telemetry_conversion` is enabled, which copies the resource attributes onto every
series as labels — without it `host.name` would only exist in `target_info` and the dashboard
could not filter by host.

| Label | Origin |
|---|---|
| `host_name`, `centreon_host_id` | resource attributes, from the global cache enrichment |
| `job` | `centreon/centreon-broker`, i.e. `service.namespace/service.name` |
| `centreon_service_description`, `centreon_service_id` | datapoint attributes |
| `centreon_metric_name` | the raw Centreon metric name, kept even when semconv mapping renamed the metric |
| `centreon_state_type` | `hard` or `soft` |
| `centreon_threshold_level` / `centreon_threshold_bound` | `warning`/`critical` and `upper`/`lower` |
| `centreon_bound_type` | `min` or `max` |

## Notes

- **Out-of-order window.** Datapoints are stamped with `last_check`, not with export time, so
  Prometheus would reject them as too old. `out_of_order_time_window` in
  [prometheus.yml](prometheus.yml) allows them. It is a **config setting, not a command line
  flag** — passing `--storage.tsdb.out-of-order-time-window` makes Prometheus exit with
  `unknown long flag`, and the only symptom you see is Grafana saying "An error occurred within
  the plugin" with every panel empty. `docker compose ps -a` is what shows the exited container.
- **`centreon_metric_bound` will be empty with the default test config.** `add_bound` is guarded by
  `std::isfinite`, and the generated `check.pl` perfdata has no min/max fields. Send a check result
  with all five fields to see it.
- **Seeing the raw payloads.** `docker compose logs -f otel-collector` prints every datapoint with
  its attributes — the same thing the suite's `${OTLP_DUMP}` file gives, without the 5-request cap.
- **Image tags float.** This is a dev stack; a pinned tag that no longer exists is worse than a
  version bump. Pin them if you need reproducibility.

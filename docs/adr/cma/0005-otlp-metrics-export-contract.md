# CMA-0005 — OTLP metrics export contract

- Status: Proposed
- Owner: @centreon/owners-cpp
- Scope: `agent/src/scheduler.cc` (`_store_result_in_metrics_and_exemplars`, `_get_scope_metrics`, `_add_metric_to_scope`, `_add_exemplar`, send timer)

## Context

Check results travel to Engine as OpenTelemetry metrics (`ExportMetricsServiceRequest` inside `MessageFromAgent.otel_request`). Engine rebuilds a Nagios-like check result from them in `engine/modules/opentelemetry/src/centreon_agent/agent_check_result_builder.cc`. The encoding is therefore a contract between the two binaries, as strict as the proto itself. Engine always sets `use_exemplar = true` (`agent_impl.cc`).

Encoding of one check result:

| OTLP element | Value |
| --- | --- |
| `ResourceMetrics.resource.attributes` | `host.name` = supervised host (local config `host`), `service.name` = `service_description` (empty for the host check) |
| `ScopeMetrics` | one per service in the current request |
| metric `status` | gauge, `as_int` = 0/1/2/3, `description` = `outputs.front()` (for `check_exec`: short output + `\n` + long output) cut at the first `|`, UTF-8 checked; exemplars `status_confirmed` (int 0/1), `current_attempt` (int) |
| one metric per perfdata | name = perfdata label, `unit` = perfdata unit, gauge `as_double`, one attribute key among `counter`, `derive`, `absolute`, `auto`, `gauge` |
| perfdata exemplars | `crit_gt`/`crit_ge`, `crit_lt`/`crit_le`, `warn_gt`/`warn_ge`, `warn_lt`/`warn_le`, `min`, `max`; `_ge`/`_le` when the threshold is inclusive (`@` range) |
| timestamps | `start_time_unix_nano` = check start, `time_unix_nano` = check end |

Batching: results accumulate in `_current_request` and are sent every `export_period` seconds (absolute time points, no drift). The request is sent early when (a) the same service already has a result in it, or (b) estimated size exceeds 2 MiB after a result is added (estimate = number of `ResourceMetrics` × average `ResourceMetrics` size, measured once when more than 10 are present; no size check before that).

## Decision

1. Metric name `status`, resource attribute keys `host.name` / `service.name`, exemplar labels listed above and perfdata type attribute keys MUST NOT be renamed. Engine parses them by name. A new piece of data is a new exemplar label or a new attribute, added on the Engine side in the same PR.
2. One `ResourceMetrics` per service per request. A second result for the same service MUST flush the current request first (rule (a)), so Engine never sees two results of the same service in one request.
3. An exemplar is emitted only when the value is set: an unset value is `NaN` (perfdata default) and is skipped by the `<= numeric_limits<double>::max()` test. That test MUST be kept for any new optional value. A `-inf` low bound (range `~:x`) passes the test and is emitted; Engine handles it.
4. Outputs put in `description` MUST go through `common::check_string_utf8`: protobuf rejects invalid UTF-8 strings and the whole request would be lost.
5. The early-flush threshold (2 MiB) MUST stay below the smallest allowed gRPC `max_message_length` (schema minimum 4 MiB, CMA-0010).
6. The export timer MUST use absolute time points (`_next_send_time_point += export_period`) and send nothing when the request is empty.
7. `use_exemplar == false` path (`_store_result_in_metrics`) is unused and empty. It MUST NOT be extended; it will be removed.

## Consequences

- Engine gets one consistent result per service per message, with Nagios semantics (state, soft/hard, attempt, thresholds) carried by exemplars.
- Third-party OTLP consumers see standard gauges but not the Centreon semantics of exemplars.

## Guardrails

- A renamed or removed metric name, attribute key or exemplar label without the Engine change.
- A perfdata pushed into an existing `ResourceMetrics` of another check result.
- A string placed in an OTLP message without UTF-8 sanitizing.
- A hard-coded size limit above 4 MiB, or sending without the size check.

## Known deviations

- `_add_metric_to_scope` chooses `warn_le` / `warn_lt` with `perf.critical_mode()` instead of `perf.warning_mode()`. Low warning thresholds in `@` mode are mislabelled when warning and critical modes differ. Fix in a dedicated PR with a unit test in `scheduler_test.cc`.
- `_store_result_in_metrics` is a commented-out stub (see rule 7).

## References

- `agent/src/scheduler.cc`, `agent/inc/com/centreon/agent/scheduler.hh`
- `engine/modules/opentelemetry/src/centreon_agent/agent_check_result_builder.cc`
- `common/inc/com/centreon/common/perfdata.hh`, `common/inc/com/centreon/common/utf8.hh`
- `agent/test/scheduler_test.cc` (`correct_output_examplar`)

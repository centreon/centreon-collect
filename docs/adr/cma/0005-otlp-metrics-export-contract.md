# CMA-0005: Encode check results as OTLP gauges carrying Centreon semantics in exemplars

- **Status:** Proposed
- **Date:** 2026-10-28

## Context

Check results reach Engine as OpenTelemetry metrics in `MessageFromAgent.otel_request`. Engine rebuilds a Nagios-like result from them in `agent_check_result_builder.cc` and parses every element by name, so the encoding is a contract as strict as the proto. Engine always sets `use_exemplar = true`.

The current encoding, in `scheduler::_store_result_in_metrics_and_exemplars`:

- one `ResourceMetrics` per service and per request, with resource attributes `host.name` (the supervised host) and `service.name` (`service_description`, empty for the host check);
- a gauge named `status`, `as_int` 0 to 3; its `description` is `outputs.front()` (for `check_exec`: short output, `\n`, long output) cut at the first `|` and passed through `common::check_string_utf8`; exemplars `status_confirmed` and `current_attempt`;
- one gauge per perfdata, named after the label, with its unit and one attribute key among `counter`, `derive`, `absolute`, `auto`, `gauge`; exemplars `crit_gt`/`crit_ge`, `crit_lt`/`crit_le`, `warn_gt`/`warn_ge`, `warn_lt`/`warn_le`, `min`, `max`, the `_ge`/`_le` form for inclusive `@` ranges;
- `start_time_unix_nano` = check start, `time_unix_nano` = check end.

Results accumulate in one request, sent every `export_period` seconds on absolute time points and only when not empty. The request is sent early when the same service already has a result in it, or when, after a result is added, the estimated size exceeds 2 MiB (number of `ResourceMetrics` × their average size, measured once more than 10 are present).

Alternatives weighed:

- A dedicated protobuf message for check results. It lost: OTLP gives Engine one ingestion path shared with other OpenTelemetry sources, and standard tools can read the gauges.
- Thresholds and state as metric attributes. It lost: attributes identify a series; exemplars attach per-point values without multiplying series.

## Decision

Keep the metric name `status`, the attribute keys `host.name` and `service.name`, the exemplar labels and the perfdata type keys unchanged. Add new data as a new exemplar label or attribute, with the Engine side in the same change.

Send at most one result per service in a request: flush the request before adding a second one. Emit an exemplar only when its value is set, keeping the `<= numeric_limits<double>::max()` test that skips `NaN`. Pass every string placed in a message through `check_string_utf8`. Keep the early-flush threshold below the smallest allowed `max_message_length` (4 MiB). Schedule exports on absolute time points.

## Consequences

- Engine receives one consistent result per service per message, with state, soft/hard, attempt and thresholds carried by exemplars.
- Third-party OTLP consumers see standard gauges, not the Centreon meaning of the exemplars.
- An unset threshold is `NaN` (perfdata default) and is skipped. A `-inf` low bound (range `~:x`) passes the test and is emitted; Engine handles it.
- Before more than 10 `ResourceMetrics` are present, no size check runs, and the early flush happens once 2 MiB is already passed.
- Known defect: `_add_metric_to_scope` picks `warn_le`/`warn_lt` with `perf.critical_mode()` instead of `perf.warning_mode()`, mislabelling low warning thresholds when the two modes differ. Fixed in a dedicated change with a test in `scheduler_test.cc`.
- The `use_exemplar == false` path, `_store_result_in_metrics`, is an empty stub. It is not extended and will be removed.

## When to Amend

- Engine consumes results through another path (Broker OTLP module, collector) that cannot read exemplars.
- The OpenTelemetry specification deprecates exemplars on gauges, or Engine needs several results per service in one request.

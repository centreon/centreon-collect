# Metrics in VictoriaMetrics: contract between Broker and centreon-web

Status: proof of concept. Broker writes, centreon-web reads; this file is the
only place both sides take the data model from. Change it first, then both.

## Writing (Centreon Broker, `victoria_metrics` output)

- Endpoint: VictoriaMetrics single-node, InfluxDB line protocol on
  `POST /write`, with timestamps in seconds declared as such
  (`?precision=s`). Optional basic authentication and TLS, as today.
- Metric points: measurement `metric`, one field `val`, so the series is
  `metric_val`. Labels:
  - `id`: `metrics.metric_id` (the key the web reads by);
  - `index_id`: `index_data.id` of the service the metric belongs to;
  - `host_id`, `serv_id`, `name` (metric name), `unit`;
  - the configured extra tags (host and service names, groups, severity...).
  - Never a value that changes often (`min`, `max`, thresholds): each change
    would start a new series.
- Status points: measurement `status`, field `val`, so the series is
  `status_val`, labels `id` = `index_data.id`, `host_id`, `serv_id`. Value
  100 for OK, 75 for WARNING, 0 for CRITICAL, as in the RRD status files;
  UNKNOWN is not written (a gap).
- Rebuild of a metric (the "rebuild" action of centreon-web, replayed by
  `unified_sql` from `data_bin`): its series is deleted, then every replayed
  point is written with its original timestamp. The status series of the
  metric's index is rebuilt the same way.
- Values are written as received: COUNTER, DERIVE and ABSOLUTE metrics are
  raw counters, not rates.
- Endpoints: `http_target` (write, default `/write`), `http_delete_target`
  (default `/api/v1/admin/tsdb/delete_series`) and `http_series_target`
  (default `/api/v1/series`, read when a rebuild needs a metric's labels).
- Removal of a metric or of a service's graphs: the matching series are
  deleted (`/api/v1/admin/tsdb/delete_series`, selector on `id` for a metric,
  on `index_id` and `id` for an index).

## Reading (centreon-web)

- `GET /api/v1/query_range` with `query=avg_over_time(metric_val{id="<metric_id>"}[<step>s])`
  (`status_val{id="<index_id>"}` for status graphs), `start`, `end` and `step`
  in seconds. The step is chosen to return at most about 500 points over the
  period, and never less than the check interval.
- Virtual metrics are computed by centreon-web on the fetched series.
- The data source type of a metric (`metrics.data_source_type`) decides
  the query: GAUGE is averaged as above; COUNTER and DERIVE are turned into
  a per-second rate (`rate(metric_val{id="<metric_id>"}[<step>s])`;
  DERIVE may be negative, COUNTER may not); ABSOLUTE is the value divided
  by the interval between points. This is what RRD does on the write side.
- Same authentication options as the write side.

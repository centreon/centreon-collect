# centreon-broker image for the gorgone-rs proof of concept

`centreon-broker-trixie:develop` plus the control API entrypoint of this checkout (`.github/docker/centreon-broker/trixie/entrypoint`: `PUT /files/{target}`, bearer token, `BROKER_WAIT_FOR_CONFIG`, see the "Broker control API" section of `gorgone-rs/docs/containers-contract.md`) and the `victoria_metrics` output of this branch (`/usr/share/centreon/lib/centreon-broker/70-victoria_metrics.so`, a module the base image does not package). The output writes metrics and status as `gorgone-rs/docs/victoria-metrics-contract.md` describes, so that centreon-web can read its graphs from VictoriaMetrics instead of RRD files:

- `POST /write?precision=s` (InfluxDB line protocol, timestamps in seconds); a configured `http_target` keeps its own `precision` if it sets one;
- `metric_val` labels `id`, `index_id`, `host_id`, `serv_id`, `name`, `unit` and the configured extra tags (no `min`/`max` by default); `status_val` labels `id` (index), `host_id`, `serv_id`; UNKNOWN is not written;
- rebuild (`RebuildMessage` of unified_sql): the series of the rebuilt metrics and of their status are deleted, then the replayed points are written with their original timestamps;
- removal (`RemoveGraphMessage`, legacy `remove_graph`): `metric_val{id=…}`, or `metric_val{index_id=…}` and `status_val{id=…}`, are deleted.

`cbd` is not replaced: the module embeds `http_tsdb` and resolves the Broker core from the `cbd` of the base image, built from the same `broker/`, `common/`, `bbdo/` and `clib/` sources (check with `git diff <org.opencontainers.image.revision of the base image> -- broker/core common bbdo clib`). If they diverge, also copy `/build/broker/cbd`.

## Build

All commands run from the repository root. The build tree lives in the docker volume `collect-poc-build-trixie`, configured as in `../engine/README.md` step 1. The build generates files into its source directory, so it runs on a copy of the sources, never on the working tree.

```bash
PACKAGING=docker.centreon.com/centreon/packaging-centreon-collect-vcpkg-trixie:f78f100f
SRC=/tmp/collect-src   # copy of the sources mounted as /src, as when the volume was configured

# 1. Sources: rsync keeps mtimes, so touch what changed or ninja keeps old objects.
rsync -a --exclude=/gorgone-rs/target --exclude=/.git ./ "$SRC"/
find "$SRC"/broker/victoria_metrics "$SRC"/broker/http_tsdb -type f -exec touch {} +

# 2. Module, cbd and unit tests.
docker run --rm -v "$SRC":/src -v collect-poc-build-trixie:/build -w /src "$PACKAGING" bash -ec '
  export VCPKG_ROOT=/vcpkg VCPKG_FORCE_SYSTEM_BINARIES=1 PATH=/vcpkg:/opt/cmake-4.3.3/bin:$PATH
  ninja -C /build -j "$(nproc)" cbd 70-victoria_metrics ut_broker
  cd /build && tests/ut_broker --gtest_filter="victoria*:Victoria*:HttpTsdb*:http_tsdb*"'

# 3. Build context, outside the working tree: the stripped module.
CTX=$(mktemp -d)
docker run --rm -v collect-poc-build-trixie:/build:ro -v "$CTX":/out "$PACKAGING" \
  sh -c 'strip -o /out/70-victoria_metrics.so /build/broker/lib/70-victoria_metrics.so && chmod 755 /out/70-victoria_metrics.so'

# 4. Image, with the entrypoint of this checkout as the named context
#    `entrypoint`. The build fails if a library is missing or if the module
#    was not built for the Broker version of the base image.
docker build -t centreon-broker-trixie:gorgone-poc -f gorgone-rs/deploy/centreon/broker/Dockerfile \
  --build-context entrypoint=.github/docker/centreon-broker/trixie/entrypoint "$CTX"
```

## Configuration

The entrypoint refuses the packaged default configuration: mount a real one and pass it to `api_control.py`, or start the container with `BROKER_WAIT_FOR_CONFIG=true` (and `BROKER_CONTROL_TOKEN`) and let the poller's `gorgone-agent` push it:

```bash
docker run -d --name broker-poc -p 8080:8080 -e BROKER_WAIT_FOR_CONFIG=true -e BROKER_CONTROL_TOKEN="$TOKEN" \
  centreon-broker-trixie:gorgone-poc
curl -s localhost:8080/health                                  # 503 {"state":"waiting_for_config"}
tar -C ctx -cf - engine-context.json | curl -s -o /dev/null -w '%{http_code}\n' -X PUT \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/x-tar' \
  --data-binary @- localhost:8080/files/engine_context            # 204, written with mode 0640
tar -C conf -cf - central-broker.json | curl -s -o /dev/null -w '%{http_code}\n' -X PUT \
  -H "Authorization: Bearer $TOKEN" -H 'Content-Type: application/x-tar' \
  --data-binary @- localhost:8080/files/broker                    # 204, cbd starts
```

Push `engine_context` before `broker`: `cbd` reads `/etc/centreon-engine/engine-context.json` when it starts and on each `POST /reload` (SIGHUP), never in between. Later pushes do not restart `cbd`: call `POST /reload`.

A `victoria_metrics` output:

```json
{
  "name": "victoria_metrics",
  "type": "victoria_metrics",
  "db_host": "victoria-metrics",
  "db_port": "8428",
  "db_user": "user",
  "db_password": "password",
  "queries_per_transaction": "1000"
}
```

| Key | Default | Use |
|---|---|---|
| `http_target` | `/write` | Write endpoint; `precision=s` is added unless the target sets a `precision` |
| `http_delete_target` | `/api/v1/admin/tsdb/delete_series` | Series deletion (rebuild, removal) |
| `http_series_target` | `/api/v1/series` | Name and unit of a rebuilt metric the output has not seen since it started |
| `account_id` | empty | Sent as the `account_id` header |

For a VictoriaMetrics cluster, the three targets are `/insert/<tenant>/influx/write`, `/delete/<tenant>/prometheus/api/v1/admin/tsdb/delete_series` and `/select/<tenant>/prometheus/api/v1/series`. `db_user` and `db_password` are mandatory; they are sent as basic authentication, which a single-node VictoriaMetrics ignores unless `-httpAuth.*` is set.

The output must run in the `cbd` that runs `unified_sql` (or receive its `metric_mapping`, `index_mapping` and rebuild messages): the index of a metric comes from the global cache, filled by the metric mappings unified_sql publishes.

```bash
docker run -d --name broker-poc -v "$PWD/central-broker.json":/etc/centreon-broker/vm-broker.json:ro \
  centreon-broker-trixie:gorgone-poc \
  /opt/centreon/venv/bin/python3 /usr/local/lib/centreon-broker/api_control.py /etc/centreon-broker/vm-broker.json
docker exec broker-poc grep victoria_metrics /var/lib/centreon-broker/central-broker.log
```

## Failures

A write that fails (connection, 5xx, 429) is kept and sent again; a 4xx is logged and dropped. A failed deletion or series lookup makes the output stop and retry after `retry_interval`: the events not acknowledged, the rebuild or removal message among them, are replayed.

#!/usr/bin/env bash
#
# podman-stack.sh — same stack as compose.yml, but running in podman on the
# network of a centpod container, for a cbd that lives inside that container.
#
#   centpod (cbd, otlp output) --OTLP/gRPC--> otel-collector:4317
#                                                   | remote write
#                                                   v
#                                             prometheus:9090
#                                                   v
#                                              grafana:3000  --> published on the host
#
# Why not compose.yml: that file assumes cbd runs on the host and publishes
# 4317 there. A rootless podman container cannot reach host-published ports
# (its netns has no route back to the host), so a docker stack on the host is
# unreachable from cbd. Joining the same podman network instead makes the
# collector resolvable by name — that name is what goes in central-broker.json.
#
# Usage:
#   ./podman-stack.sh                      # start, defaults below
#   NETWORK=centreon-net-25.10-debian12 GRAFANA_PORT=3000 ./podman-stack.sh
#   ./podman-stack.sh down                 # remove the three containers
#
set -euo pipefail

NETWORK="${NETWORK:-centreon-net-25.10-debian12}"
GRAFANA_PORT="${GRAFANA_PORT:-3000}"
# Published only so you can send test payloads from the host; cbd does not use
# it, it talks to otel-collector:4317 over the podman network. Not 14317: that
# one is already held by the centpod container's own publish.
COLLECTOR_PORT="${COLLECTOR_PORT:-54317}"
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

COLLECTOR=centreon-otel-collector
PROMETHEUS=centreon-prometheus
GRAFANA=centreon-grafana

down() {
  for c in "$COLLECTOR" "$PROMETHEUS" "$GRAFANA"; do
    podman rm -f --ignore "$c" >/dev/null 2>&1 || true
  done
}

if [[ "${1:-up}" == "down" ]]; then
  down
  echo "stack removed (volumes centreon-prom-data / centreon-grafana-data kept)"
  exit 0
fi

podman network exists "$NETWORK" || {
  echo "network '$NETWORK' does not exist — set NETWORK to the one your centpod container uses:" >&2
  podman network ls >&2
  exit 1
}

down

# --user 0 for prometheus and grafana: rootless podman maps container root to
# your host user, so the named volumes are writable. Their default users
# (nobody / 472) map to subuids that own nothing.
podman run -d --replace --name "$PROMETHEUS" \
  --network "$NETWORK" --network-alias prometheus \
  --user 0 \
  -v "$HERE/prometheus.yml:/etc/prometheus/prometheus.yml:ro" \
  -v centreon-prom-data:/prometheus \
  -p "9090:9090" \
  docker.io/prom/prometheus:latest \
  --config.file=/etc/prometheus/prometheus.yml \
  --web.enable-remote-write-receiver \
  --storage.tsdb.retention.time=24h

# DUMP_DIR=/somewhere adds the file exporter overlay, so every request Broker
# sends is appended as OTLP JSON to $DUMP_DIR/otlp.json — the encoding
# OtlpCollector.py produces with MessageToDict, without needing a Robot run.
DUMP_ARGS=()
if [[ -n "${DUMP_DIR:-}" ]]; then
  mkdir -p "$DUMP_DIR"
  # --user 0: the image runs as uid 10001, which maps to a subuid owning
  # nothing, so it could not create the file in a dir owned by you.
  DUMP_ARGS=(--user 0
             -v "$DUMP_DIR:/dump:z"
             -v "$HERE/otel-collector-dump.yml:/etc/otel/dump.yml:ro")
fi

podman run -d --replace --name "$COLLECTOR" \
  --network "$NETWORK" --network-alias otel-collector \
  -v "$HERE/otel-collector.yml:/etc/otel/config.yml:ro" \
  "${DUMP_ARGS[@]}" \
  -p "${COLLECTOR_PORT}:4317" \
  docker.io/otel/opentelemetry-collector-contrib:latest \
  --config=/etc/otel/config.yml \
  ${DUMP_DIR:+--config=/etc/otel/dump.yml}

podman run -d --replace --name "$GRAFANA" \
  --network "$NETWORK" --network-alias grafana \
  --user 0 \
  -e GF_AUTH_ANONYMOUS_ENABLED=true \
  -e GF_AUTH_ANONYMOUS_ORG_ROLE=Admin \
  -e GF_AUTH_DISABLE_LOGIN_FORM=true \
  -e GF_USERS_DEFAULT_THEME=dark \
  -v "$HERE/grafana/provisioning:/etc/grafana/provisioning:ro" \
  -v "$HERE/grafana/dashboards:/var/lib/grafana/dashboards:ro" \
  -v centreon-grafana-data:/var/lib/grafana \
  -p "${GRAFANA_PORT}:3000" \
  docker.io/grafana/grafana:latest

cat <<EOF

stack up on podman network '$NETWORK'

  grafana     http://localhost:${GRAFANA_PORT}   (anonymous admin)
  prometheus  http://localhost:9090
  collector   otel-collector:4317 inside the network, localhost:${COLLECTOR_PORT} from the host

In /etc/centreon-broker/central-broker.json of the centpod container:

  "endpoint": "otel-collector:4317"

then: podman exec <centpod> systemctl restart cbd
EOF

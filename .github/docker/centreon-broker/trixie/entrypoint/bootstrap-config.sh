#!/bin/sh

# Gorgone-less central cbd bootstrap (PoC).
#
# Usage: bootstrap-config.sh <config.json>
#
# When the cbd config file is absent or still a package default (seeded into an
# empty volume by Docker copy-up) and CENTREON_BROKER_API_TOKEN is set, render a
# minimal central config: remote_api on 8099 with the bearer token, BBDO gRPC
# TLS input on 5669 with the self-generated identity (tls-self-ca.sh), no
# output. The web then delivers the real configuration through the API (first
# central export) and cbd reloads itself. A delivered file is never overwritten.
#
# Env: CENTREON_BROKER_API_TOKEN, BROKER_NAME (central-broker-master),
# BROKER_POLLER_ID (1), BROKER_POLLER_NAME (Central), BROKER_CERTS_DIR
# (/etc/centreon-broker/certs), BROKER_API_PORT (8099), BROKER_BBDO_PORT (5669).

CFG="$1"
DEFAULTS=/usr/share/centreon-broker/defaults.sha256

[ -n "$CFG" ] || exit 0
[ -n "${CENTREON_BROKER_API_TOKEN:-}" ] || exit 0
if [ -f "$CFG" ]; then
    SUM=$(sha256sum "$CFG" | cut -d' ' -f1)
    if ! grep -q "^$SUM " "$DEFAULTS" 2>/dev/null; then
        echo "bootstrap: $CFG exists, keeping the delivered configuration"
        exit 0
    fi
fi

BROKER_NAME="${BROKER_NAME:-central-broker-master}"
BROKER_POLLER_ID="${BROKER_POLLER_ID:-1}"
BROKER_POLLER_NAME="${BROKER_POLLER_NAME:-Central}"
CERTS="${BROKER_CERTS_DIR:-/etc/centreon-broker/certs}"
API_PORT="${BROKER_API_PORT:-8099}"
BBDO_PORT="${BROKER_BBDO_PORT:-5669}"

case "$BROKER_POLLER_ID" in
    ''|*[!0-9]*)
        echo "bootstrap: ERROR: BROKER_POLLER_ID must be a decimal integer" >&2
        exit 1
        ;;
esac
case "$CENTREON_BROKER_API_TOKEN" in
    *\"*|*\\*)
        echo "bootstrap: ERROR: CENTREON_BROKER_API_TOKEN must not contain" \
             "'\"' or '\\'" >&2
        exit 1
        ;;
esac
if [ ! -r "$CERTS/central.crt" ] || [ ! -r "$CERTS/central.key" ]; then
    echo "bootstrap: ERROR: $CERTS/central.crt or central.key missing" \
         "(BROKER_TLS_SELF_CA=0 needs a provided identity)" >&2
    exit 1
fi

echo "bootstrap: rendering a minimal central configuration in $CFG" \
     "(remote_api :$API_PORT, bbdo grpc tls :$BBDO_PORT)"

set +x
umask 027
cat > "$CFG.tmp" <<EOF
{
  "centreonBroker": {
    "broker_id": 1,
    "broker_name": "${BROKER_NAME}",
    "poller_id": ${BROKER_POLLER_ID},
    "poller_name": "${BROKER_POLLER_NAME}",
    "module_directory": "/usr/share/centreon/lib/centreon-broker",
    "log_timestamp": true,
    "event_queue_max_size": 100000,
    "cache_directory": "/var/lib/centreon-broker",
    "bbdo_version": "3.1.0",
    "log": {
      "directory": "/var/log/centreon-broker/",
      "filename": "${BROKER_NAME}.log",
      "max_size": 0,
      "loggers": {"core": "info", "bbdo": "info", "tls": "info", "grpc": "error"}
    },
    "remote_api": {
      "listen_address": "0.0.0.0",
      "port": "${API_PORT}",
      "token": "${CENTREON_BROKER_API_TOKEN}"
    },
    "input": [
      {
        "name": "${BROKER_NAME}-input",
        "type": "bbdo_server",
        "transport_protocol": "grpc",
        "host": "0.0.0.0",
        "port": "${BBDO_PORT}",
        "encryption": "yes",
        "certificate": "${CERTS}/central.crt",
        "private_key": "${CERTS}/central.key"
      }
    ]
  }
}
EOF
chmod 0640 "$CFG.tmp"
mv "$CFG.tmp" "$CFG"
touch "$(dirname "$CFG")/.bootstrap"

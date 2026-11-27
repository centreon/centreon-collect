#!/bin/sh

# Gorgone-less poller bootstrap (PoC).
#
# A new poller starts with an empty configuration. When POLLER_UID is set and
# no centengine.cfg exists yet, render a minimal Engine configuration (no host)
# and the cbmod configuration that connects to the central Broker over gRPC
# with TLS (central identity checked against BROKER_CA_CERT) and accepts
# downstream remote actions. The central then delivers the real configuration
# over the BBDO stream (first export from the web). An existing file is never
# overwritten.
#
# At every start (POLLER_UID set), the CA is verified (BROKER_CA_FINGERPRINT)
# and copied to /etc/centreon-broker/certs/ca.crt, the one path the delivered
# cbmod configuration names; a cbmod output with an unreadable CA file is
# reported as an error.
#
# Env: CENTRAL_BROKER_HOST, CENTRAL_BROKER_PORT (5669), POLLER_NAME, POLLER_ID,
# POLLER_UID, BROKER_CA_CERT (source CA file, default
# /etc/centreon-broker/certs/ca.crt), BROKER_TLS_HOSTNAME (default
# CENTRAL_BROKER_HOST), BROKER_CA_PEM (CA content, written to BROKER_CA_CERT if
# absent), BROKER_CA_FINGERPRINT (SHA-256 of the CA DER, fail closed on
# mismatch).

ENGINE_DIR="/etc/centreon-engine"
BROKER_DIR="/etc/centreon-broker"
CFG="$ENGINE_DIR/centengine.cfg"
DEFAULTS="/usr/share/centreon-engine/defaults.sha256"

# A package-default file (seeded into an empty volume by Docker copy-up)
# counts as absent.
is_set() {
    [ -f "$1" ] || return 1
    sum=$(sha256sum "$1" | cut -d' ' -f1)
    ! grep -q "^$sum " "$DEFAULTS" 2>/dev/null
}

if [ -z "${POLLER_UID:-}" ]; then
    return 0 2>/dev/null || exit 0
fi

# One CA path for every poller (contracts.md §11.C5): the delivered cbmod
# configuration always names $CA_CANON. The CA may come from BROKER_CA_PEM,
# or from another file (BROKER_CA_CERT, e.g. a read-only mount by the install
# script); it is verified, then copied to $CA_CANON at every start.
CA_CANON="$BROKER_DIR/certs/ca.crt"
BROKER_CA_CERT="${BROKER_CA_CERT:-$CA_CANON}"

# CA handed out by the poller creation API (contracts.md §8.3): written only
# if no CA file exists (an operator file wins).
if [ -n "${BROKER_CA_PEM:-}" ]; then
    if [ ! -f "$BROKER_CA_CERT" ]; then
        mkdir -p "$(dirname "$BROKER_CA_CERT")"
        printf '%s\n' "$BROKER_CA_PEM" > "$BROKER_CA_CERT"
        chmod 0644 "$BROKER_CA_CERT"
        echo "bootstrap: CA written to $BROKER_CA_CERT from BROKER_CA_PEM"
    elif [ "$(cat "$BROKER_CA_CERT")" != "$(printf '%s' "$BROKER_CA_PEM")" ]; then
        echo "bootstrap: WARNING: $BROKER_CA_CERT exists and differs from" \
             "BROKER_CA_PEM, keeping the file"
    fi
fi

# Optional pin of the CA actually used: SHA-256 of the DER of its first
# certificate. Checked at every start, fail closed on mismatch.
if [ -n "${BROKER_CA_FINGERPRINT:-}" ]; then
    want=$(printf '%s' "$BROKER_CA_FINGERPRINT" | tr -d ': ' | tr 'A-F' 'a-f')
    got=$(awk '/-----BEGIN CERTIFICATE-----/{p=1;next} /-----END CERTIFICATE-----/{if(p)exit} p' \
              "$BROKER_CA_CERT" 2>/dev/null | tr -d ' \r\n' | base64 -d 2>/dev/null \
          | sha256sum | cut -d' ' -f1)
    if [ ! -r "$BROKER_CA_CERT" ] || [ "$got" != "$want" ]; then
        echo "bootstrap: ERROR: CA fingerprint mismatch for $BROKER_CA_CERT" \
             "(expected $want, got ${got:-none}), refusing to start" >&2
        exit 1
    fi
    echo "bootstrap: CA fingerprint verified ($want)"
fi

if [ "$BROKER_CA_CERT" != "$CA_CANON" ] && [ -r "$BROKER_CA_CERT" ]; then
    if ! cmp -s "$BROKER_CA_CERT" "$CA_CANON"; then
        if mkdir -p "$(dirname "$CA_CANON")" &&
           cp "$BROKER_CA_CERT" "$CA_CANON.tmp" &&
           chmod 0644 "$CA_CANON.tmp" &&
           mv "$CA_CANON.tmp" "$CA_CANON"; then
            echo "bootstrap: CA copied from $BROKER_CA_CERT to $CA_CANON"
        else
            rm -f "$CA_CANON.tmp"
            echo "bootstrap: ERROR: cannot copy the CA from $BROKER_CA_CERT" \
                 "to $CA_CANON, refusing to start" >&2
            exit 1
        fi
    fi
fi

if [ ! -r "$CA_CANON" ]; then
    echo "bootstrap: WARNING: CA certificate $CA_CANON not readable," \
         "the poller will not accept remote actions"
fi

# A cbmod output whose CA file is unreadable makes Engine stop at start (the
# gRPC endpoint cannot be created): say it here, on the container log.
for m in "$BROKER_DIR"/*module.json; do
    [ -f "$m" ] || continue
    sed -n 's/.*"ca_certificate"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$m" |
    while read -r ca; do
        if [ -n "$ca" ] && [ ! -r "$ca" ]; then
            echo "bootstrap: ERROR: $m: ca_certificate '$ca' is not readable," \
                 "cbmod cannot create its output and Engine will stop" >&2
        fi
    done
done

if is_set "$CFG"; then
    echo "bootstrap: $CFG exists, keeping the delivered configuration"
    return 0 2>/dev/null || exit 0
fi

: "${CENTRAL_BROKER_HOST:?bootstrap needs CENTRAL_BROKER_HOST}"
: "${POLLER_NAME:?bootstrap needs POLLER_NAME}"
CENTRAL_BROKER_PORT="${CENTRAL_BROKER_PORT:-5669}"
POLLER_ID="${POLLER_ID:-$POLLER_UID}"
BROKER_TLS_HOSTNAME="${BROKER_TLS_HOSTNAME:-$CENTRAL_BROKER_HOST}"
MODULE_CFG="$BROKER_DIR/${POLLER_NAME}-module.json"
MODULES_DIR="/usr/lib64/centreon-engine"
[ -d "$MODULES_DIR" ] || MODULES_DIR="/usr/lib/centreon-engine"

echo "bootstrap: rendering a minimal configuration for poller" \
     "'$POLLER_NAME' (uid $POLLER_UID) -> $CENTRAL_BROKER_HOST:$CENTRAL_BROKER_PORT"

mkdir -p "$ENGINE_DIR" "$BROKER_DIR" /var/lib/centreon-engine/rw \
    /var/log/centreon-engine

[ -f "$ENGINE_DIR/bootstrap-objects.cfg" ] || cat > "$ENGINE_DIR/bootstrap-objects.cfg" <<'EOF'
define timeperiod {
    timeperiod_name     24x7
    alias               24x7
    sunday              00:00-24:00
    monday              00:00-24:00
    tuesday             00:00-24:00
    wednesday           00:00-24:00
    thursday            00:00-24:00
    friday              00:00-24:00
    saturday            00:00-24:00
}

define command {
    command_name        bootstrap-dummy
    command_line        /bin/true
}
EOF

cat > "$CFG" <<EOF
# Bootstrap configuration rendered by 03-bootstrap-config.sh, replaced by the
# first configuration delivered by the central.
log_file=/var/log/centreon-engine/centengine.log
log_v2_logger=file
cfg_file=$ENGINE_DIR/bootstrap-objects.cfg
status_file=/var/log/centreon-engine/status.dat
check_external_commands=1
command_check_interval=1s
command_file=/var/lib/centreon-engine/rw/centengine.cmd
event_broker_options=-1
broker_module=$MODULES_DIR/externalcmd.so
broker_module_cfg_file=$MODULE_CFG
rpc_listen_address=127.0.0.1
log_external_commands=1
retain_state_information=1
state_retention_file=/var/lib/centreon-engine/retention.dat
retention_update_interval=60
interval_length=60
EOF

if ! is_set "$MODULE_CFG"; then
    cat > "$MODULE_CFG" <<EOF
{
  "centreonBroker": {
    "broker_id": 1,
    "broker_name": "${POLLER_NAME}-module",
    "poller_id": ${POLLER_ID},
    "uid": ${POLLER_UID},
    "poller_name": "${POLLER_NAME}",
    "module_directory": "/usr/share/centreon/lib/centreon-broker",
    "log_timestamp": true,
    "event_queue_max_size": 100000,
    "cache_directory": "/var/lib/centreon-engine",
    "bbdo_version": "3.1.0",
    "remote_actions": "yes",
    "log": {
      "directory": "/var/log/centreon-broker/",
      "filename": "",
      "max_size": 0,
      "loggers": {"core": "info", "bbdo": "info", "neb": "info", "tls": "info", "grpc": "error"}
    },
    "output": [
      {
        "name": "${POLLER_NAME}-output",
        "type": "bbdo_client",
        "transport_protocol": "grpc",
        "host": "${CENTRAL_BROKER_HOST}",
        "port": "${CENTRAL_BROKER_PORT}",
        "encryption": "yes",
        "ca_certificate": "${CA_CANON}",
        "ca_name": "${BROKER_TLS_HOSTNAME}"
      }
    ]
  }
}
EOF
fi

touch "$ENGINE_DIR/.bootstrap"

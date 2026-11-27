#!/bin/sh

set -e
[ "${DEBUG:-0}" = "1" ] && set -x

rm -f /tmp/docker.ready

export PATH="/opt/centreon/venv/bin:$PATH"

# Private engine-context.json for db_password decryption, optional.
if [ -n "$APP_SECRET" ] && [ -n "$SALT" ]; then
    was_tracing=0
    case "$-" in *x*) was_tracing=1 ;; esac
    set +x
    ENGINE_CONTEXT="/etc/centreon-engine/engine-context.json"
    rm -f "$ENGINE_CONTEXT"
    printf '{"app_secret":"%s","salt":"%s"}\n' "$APP_SECRET" "$SALT" > "$ENGINE_CONTEXT"
    chmod 640 "$ENGINE_CONTEXT"
    if [ "$was_tracing" = "1" ]; then
        set -x
    fi
    echo "Engine secrets written to $ENGINE_CONTEXT"
fi

# Central PKI for the BBDO gRPC TLS input: generated once, then kept.
/usr/local/lib/centreon-broker/tls-self-ca.sh

# First central config from env when none was delivered yet (gorgone-less
# PoC): no-op without CENTREON_BROKER_API_TOKEN or with a delivered file.
for arg in "$@"; do
    case "$arg" in
        *.json)
            /usr/local/lib/centreon-broker/bootstrap-config.sh "$arg"
            break
            ;;
    esac
done

# Find the config path in "$@": refuse to start on the untouched package
# default (no SQL/storage output, would run but persist nothing), else tail
# its log file.
DEFAULTS=/usr/share/centreon-broker/defaults.sha256
for arg in "$@"; do
    case "$arg" in
        *.json)
            SUM=$(sha256sum "$arg" 2>/dev/null | cut -d' ' -f1)
            if [ -n "$SUM" ] && grep -q "^$SUM " "$DEFAULTS" 2>/dev/null; then
                echo "ERROR: $arg is still the package-default config (no SQL/storage output) - mount a real broker config" >&2
                exit 1
            fi
            # BROKER_LOG_STDOUT=true: symlink to stdout instead of tailing.
            if [ "${BROKER_LOG_STDOUT:-false}" = "true" ]; then
                /usr/local/lib/centreon-broker/log-tail.sh --link-stdout "$arg"
            else
                /usr/local/lib/centreon-broker/log-tail.sh "$arg" &
            fi
            break
            ;;
    esac
done

touch /tmp/docker.ready
echo "Centreon is ready"

exec "$@"

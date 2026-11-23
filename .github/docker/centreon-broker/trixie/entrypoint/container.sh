#!/bin/sh

set -e
[ "${DEBUG:-0}" = "1" ] && set -x

rm -f /tmp/docker.ready

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

# Find the config path in "$@": wait until it is no longer the untouched
# package default (no SQL/storage output, would run but persist nothing) -
# gorgone delivers the real config asynchronously on export - then tail its
# log file. No fingerprint available: just being non-empty is enough.
DEFAULT_SUM=$(cat /var/lib/centreon-broker/.default-config.sha256 2>/dev/null || true)
trap 'exit 0' TERM INT
for arg in "$@"; do
    case "$arg" in
        *.json)
            waited=0
            while :; do
                if [ -s "$arg" ] && { [ -z "$DEFAULT_SUM" ] || [ "$(sha256sum "$arg" 2>/dev/null | cut -d' ' -f1)" != "$DEFAULT_SUM" ]; }; then
                    break
                fi
                [ $((waited % 60)) -eq 0 ] && echo "Waiting for $arg (delivered by gorgone on export)..."
                sleep 2
                waited=$((waited + 2))
            done
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
trap - TERM INT

touch /tmp/docker.ready
echo "Centreon is ready"

exec "$@"

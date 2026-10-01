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

# Find the config path in "$@": refuse to start on the untouched package
# default (no SQL/storage output, would run but persist nothing), else tail
# its log file.
DEFAULT_SUM=$(cat /var/lib/centreon-broker/.default-config.sha256 2>/dev/null || true)
for arg in "$@"; do
    case "$arg" in
        *.json)
            if [ -n "$DEFAULT_SUM" ] && [ "$(sha256sum "$arg" 2>/dev/null | cut -d' ' -f1)" = "$DEFAULT_SUM" ]; then
                echo "ERROR: $arg is still the package-default config (no SQL/storage output) - mount a real broker config" >&2
                exit 1
            fi
            /var/lib/centreon-broker/log-tail.sh "$arg" &
            break
            ;;
    esac
done

touch /tmp/docker.ready
echo "Centreon is ready"

exec "$@"

#!/bin/sh

set -e
[ "${DEBUG:-0}" = "1" ] && set -x

rm -f /tmp/docker.ready

# Pid of the cbd started below, for its reload (SIGHUP) and stop (SIGTERM),
# by the gorgone-agent of this container among others. Written just before
# cbd starts: a signal sent earlier would end this script instead.
PID_FILE=/var/lib/centreon-broker/cbd.pid
rm -f "$PID_FILE"

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

# gorgone-agent, when the image has one (the gorgone-rs bench adds it): it
# receives the configuration and the key from gorgone-server, and reloads or
# stops cbd with a signal (see gorgone-rs/docs/containers-contract.md). Run
# again if it ends; it ends with the container.
GORGONE_AGENT_CONFIG="${GORGONE_AGENT_CONFIG:-/usr/local/lib/centreon-broker/gorgone-agent.toml}"
if [ -x /usr/bin/gorgone-agent ] && [ -f "$GORGONE_AGENT_CONFIG" ]; then
    (
        while :; do
            /usr/bin/gorgone-agent --config "$GORGONE_AGENT_CONFIG" || true
            echo "gorgone-agent ended, starting it again" >&2
            sleep 1
        done
    ) &
fi

# Find the config path in "$@": refuse to start on the untouched package
# default (no SQL output, would run but persist nothing), else tail its log
# file. BROKER_WAIT_FOR_CONFIG=true: wait until the configuration is pushed
# (by gorgone-agent) instead, then start cbd on it.
DEFAULT_SUM=$(cat /var/lib/centreon-broker/.default-config.sha256 2>/dev/null || true)
is_default() {
    [ -n "$DEFAULT_SUM" ] && [ "$(sha256sum "$1" 2>/dev/null | cut -d' ' -f1)" = "$DEFAULT_SUM" ]
}
for arg in "$@"; do
    case "$arg" in
        *.json)
            if is_default "$arg"; then
                if [ "${BROKER_WAIT_FOR_CONFIG:-false}" != "true" ]; then
                    echo "ERROR: $arg is still the package-default config (no SQL output) - mount a real broker config" >&2
                    exit 1
                fi
                echo "$arg is still the package-default config, waiting for a configuration push"
                touch /tmp/docker.ready
                while is_default "$arg"; do
                    sleep 1
                done
                echo "$arg pushed, starting cbd"
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

# exec keeps this pid.
echo $$ > "$PID_FILE"
exec "$@"

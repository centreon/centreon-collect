#!/bin/sh

set -e
[ "${DEBUG:-0}" = "1" ] && set -x

rm -f /tmp/docker.ready

export PATH="/opt/centreon/venv/bin:$PATH"

# Optional: write this broker's own PRIVATE copy of engine-context.json from
# APP_SECRET/SALT, so cbd can decrypt an AES-encrypted db_password in its own
# config (broker/core/src/main.cc::reload_engine_context()). Deliberately NOT
# a shared volume with a real centengine container, to keep broker and engine
# decoupled - /etc/centreon-engine is baked into this image instead (see
# Dockerfile), owned by the centreon-engine group with group-write, and
# centreon-broker is already a member of that group, so no root/su needed to
# write here. Skipped silently when unset: reload_engine_context() itself
# tolerates a missing file, only needed when db_password is actually encrypted.
# Guarded against DEBUG=true's `set -x` below tracing the secret values
# themselves into `docker logs`.
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

# cbd/cbwd always take their JSON config as a plain positional argument
# (broker/core/src/main.cc:303's own usage string: "[-s <poolsize>] [-c] [-D]
# [-h] [-v] [<configfile>]") — find it among "$@" so log-tail.sh can derive
# the real log path from it, rather than assuming a fixed one.
for arg in "$@"; do
    case "$arg" in
        *.json)
            /var/lib/centreon-broker/log-tail.sh "$arg" &
            break
            ;;
    esac
done

touch /tmp/docker.ready
echo "Centreon is ready"

exec "$@"

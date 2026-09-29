#!/bin/sh

set -e
[ "${DEBUG:-0}" = "1" ] && set -x

rm -f /tmp/docker.ready

export PATH="/opt/centreon/venv/bin:$PATH"

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

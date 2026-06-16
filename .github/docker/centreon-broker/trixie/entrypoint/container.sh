#!/bin/sh

set -e
[ "${DEBUG:-0}" = "1" ] && set -x

rm -f /tmp/docker.ready

export PATH="/opt/centreon/venv/bin:$PATH"

touch /tmp/docker.ready
echo "Centreon is ready"

exec "$@"

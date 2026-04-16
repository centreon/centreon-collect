#!/bin/sh

touch /tmp/docker.ready
echo "Centreon Engine is ready"

# Forward the named pipe to Docker stdout — no file accumulation.
# 00-init.sh creates the FIFO; this reader must start before exec so centengine
# can open the write end without blocking.
sed 's/^/[ENGINE-LOG] /' < /var/log/centreon-engine/centengine.log &

exec "$@"

#!/bin/sh

touch /tmp/docker.ready
echo "Centreon Engine is ready"

# centengine logs directly to stdout (log_v2_logger=stdout set by 05-engine-config.sh)
# so no tailing or piping needed — docker logs captures everything natively.
exec "$@"

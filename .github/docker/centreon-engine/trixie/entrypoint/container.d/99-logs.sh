#!/bin/sh

touch /tmp/docker.ready
echo "Centreon Engine is ready"

# centengine logs to a file (log_v2_logger=file); 10-log-tail_background.sh
# streams that file to stdout, so docker logs shows it too.
exec "$@"

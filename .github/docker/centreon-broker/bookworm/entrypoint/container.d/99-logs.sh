#!/bin/sh

touch /tmp/docker.ready
echo "Centreon is ready"

# tail -f \
#   /var/log/centreon-engine/centengine.log

exec "$@"

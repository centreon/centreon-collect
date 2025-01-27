#!/bin/sh

touch /tmp/docker.ready
echo "Centreon is ready"

tail -f \
  /var/log/centreon-engine/centengine.log \
  /var/log/centreon-broker/*.log \
  /var/log/centreon-gorgone/gorgoned.log

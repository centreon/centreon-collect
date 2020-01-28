#!/bin/sh

set -x

# Run MySQL.
service mysql start

# Run Centreon MAP for CentOS 6/7.
service centreon-map.service start

# Wait for log file to be created.
while [ \! -e /var/log/centreon-studio/centreon-studio.log ] ; do
  echo "Waiting for Centreon Studio log file..."
  sleep 2
done
tailf /var/log/centreon-studio/centreon-studio.log

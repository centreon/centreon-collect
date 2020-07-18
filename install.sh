#!/bin/bash

chown -R apache:apache /etc/centreon-broker /etc/centreon-engine

chown -R centreon-broker:centreon-broker /var/log/centreon-broker
chown -R centreon-broker:centreon-broker /var/lib/centreon-broker

chown -R centreon-engine:centreon-engine /var/log/centreon-engine
chown -R centreon-engine:centreon-engine /var/lib/centreon-engine

if [ -f /tmp/central-broker.log ] ; then
  rm -f /tmp/central-*
fi

#!/bin/sh

getent group centreon-broker > /dev/null 2>&1 || groupadd -r centreon-broker 2> /dev/null || :
getent passwd centreon-broker > /dev/null 2>&1 || useradd -m -g centreon-broker -d /var/lib/centreon-broker -r centreon-broker 2> /dev/null || :

if id centreon > /dev/null 2>&1; then
  usermod -a -G centreon-broker centreon
fi

if id centreon-gorgone > /dev/null 2>&1; then
  usermod -a -G centreon-gorgone centreon-broker
fi

if id centreon-engine > /dev/null 2>&1; then
  usermod -a -G centreon-broker centreon-engine
  usermod -a -G centreon-engine centreon-broker
fi

if id nagios > /dev/null 2>&1; then
  usermod -a -G centreon-broker nagios
fi

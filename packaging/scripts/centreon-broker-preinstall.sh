#!/bin/sh

getent group centreon-broker > /dev/null 2>&1 || groupadd -r centreon-broker 2> /dev/null || :
getent passwd centreon-broker > /dev/null 2>&1 || useradd -m -g centreon-broker -d /var/lib/centreon-broker -r centreon-broker 2> /dev/null || :


if  [ "$1" = "install" ]; then # deb
  if [ "$(getent passwd www-data)" ]; then
    usermod -a -G centreon-broker www-data
  fi
else # rpm
  if [ "$(getent passwd apache)" ]; then
    usermod -a -G centreon-broker apache
  fi
fi

if [ "$(getent passwd centreon-gorgone)" ]; then
  usermod centreon-broker -a -G centreon-gorgone
  usermod centreon-gorgone -a -G centreon-broker
fi

if [ "$(getent passwd centreon-engine)" ]; then
  usermod -a -G centreon-broker centreon-engine
  usermod -a -G centreon-engine centreon-broker
fi

if [ "$(getent passwd nagios)" ]; then
  usermod -a -G centreon-broker nagios
fi

#!/bin/sh

if ! id centreon-engine > /dev/null 2>&1; then
  useradd -d /var/lib/centreon-engine -r centreon-engine > /dev/null 2>&1
fi

if id centreon-broker > /dev/null 2>&1; then
  usermod -a -G centreon-engine centreon-broker
fi

if id centreon-gorgone > /dev/null 2>&1; then
  usermod centreon-engine -a -G centreon-gorgone
  usermod centreon-gorgone -a -G centreon-engine
fi

if id -g nagios > /dev/null 2>&1; then
  usermod -a -G centreon-engine nagios
fi

if  [ "$1" = "install" ]; then # deb
  if id -g www-data > /dev/null 2>&1; then
    usermod -a -G centreon-engine www-data
  fi
else # rpm
  if id -g apache > /dev/null 2>&1; then
    usermod -a -G centreon-engine apache
  fi
fi

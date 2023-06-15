#!/bin/sh

if ! id centreon-engine &>/dev/null; then
  useradd -d /var/lib/centreon-engine -r centreon-engine &>/dev/null
fi

if id centreon-broker &>/dev/null; then
  usermod -a -G centreon-engine centreon-broker
fi

if id centreon-gorgone &>/dev/null; then
  usermod -a -G centreon-gorgone centreon-engine
fi

if id -g nagios &>/dev/null; then
  usermod -a -G centreon-engine nagios
fi

if  [ "$1" = "configure" ]; then # deb
  if id -g www-data &>/dev/null; then
    usermod -a -G centreon-engine www-data
  fi
else # rpm
  if id -g apache &>/dev/null; then
    usermod -a -G centreon-engine apache
  fi
fi


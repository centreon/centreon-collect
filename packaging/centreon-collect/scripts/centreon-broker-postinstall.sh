#!/bin/sh

# on debian, it is needed to recreate centreon-broker user at each upgrade because it is removed on postrm step on versions < 23.10
if [ "$1" = "configure" ] ; then
  if [ ! "$(getent passwd centreon-broker)" ]; then
    adduser --system --group --home /var/lib/centreon-broker --no-create-home centreon-broker
  fi

  if [ "$(getent passwd www-data)" ]; then
    usermod -a -G centreon-broker www-data
  fi

  if [ "$(getent passwd centreon-gorgone)" ]; then
    usermod -a -G centreon-gorgone centreon-broker
  fi

  if [ "$(getent passwd centreon-engine)" ]; then
    usermod -a -G centreon-broker centreon-engine
    usermod -a -G centreon-engine centreon-broker
  fi

  if [ "$(getent passwd nagios)" ]; then
    usermod -a -G centreon-broker nagios
  fi

  chown -R centreon-broker:centreon-broker \
    /etc/centreon-broker \
    /var/lib/centreon-broker \
    /var/log/centreon-broker
  chmod -R g+w \
    /etc/centreon-broker \
    /var/lib/centreon-broker \
    /var/log/centreon-broker
fi

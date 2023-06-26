#!/bin/sh

# on debian, it is needed to recreate centreon-broker user at each upgrade because it is removed on postrm step on versions < 23.10
if [ "$1" = "configure" ] ; then
  if [ ! "$(getent passwd centreon-broker)" ]; then
    adduser --system --group --home /var/lib/centreon-broker --no-create-home centreon-broker
  fi
  if [ "$(getent passwd centreon)" ]; then
    usermod -a -G centreon-broker centreon
    usermod -a -G centreon centreon-broker
  fi
fi

chown -R centreon-broker:centreon-broker \
  /etc/centreon-broker \
  /var/lib/centreon-broker \
  /var/log/centreon-broker
chmod -R g+w \
  /etc/centreon-broker \
  /var/lib/centreon-broker \
  /var/log/centreon-broker

#!/bin/sh

startCentagent() {
  systemctl daemon-reload ||:
  systemctl unmask centagent.service ||:
  systemctl preset centagent.service ||:
  systemctl enable centagent.service ||:
  systemctl restart centagent.service ||:
}

# on debian, it is needed to recreate centreon-agent user at each upgrade because it is removed on postrm step on versions < 23.10
if [ "$1" = "configure" ] ; then
  if [ ! "$(getent passwd centreon-agent)" ]; then
    adduser --system --group --shell /bin/bash --no-create-home centreon-agent
  fi
  if [ "$(getent passwd nagios)" ]; then
    usermod -a -G centreon-agent nagios
  fi
  chown -R centreon-agent:centreon-agent \
    /etc/centreon-agent \
    /var/log/centreon-agent
fi

startCentagent


#!/bin/sh

# on debian, it is needed to recreate centreon-engine user at each upgrade because it is removed on postrm step on versions < 23.10
if [ "$1" = "configure" ] ; then
  if [ ! "$(getent passwd centreon-engine)" ]; then
    adduser --system --group --home /var/lib/centreon-engine --shell /bin/bash --no-create-home centreon-engine
  fi
  if [ "$(getent passwd centreon)" ]; then
    usermod -a -G centreon-engine centreon
    usermod -a -G centreon centreon-engine
  fi
  if [ "$(getent passwd centreon-broker)" ]; then
    usermod -a -G centreon-engine centreon-broker
  fi
  if [ "$(getent passwd centreon-gorgone)" ]; then
    usermod -a -G centreon-engine centreon-gorgone
    usermod -a -G centreon-gorgone centreon-engine
  fi
  if [ "$(getent passwd www-data)" ]; then
    usermod -a -G centreon-engine www-data
  fi
  if [ "$(getent passwd nagios)" ]; then
    usermod -a -G centreon-engine nagios
  fi
fi

mkdir -p \
  /etc/centreon-engine \
  /var/lib/centreon-engine/rw \
  /var/log/centreon-engine

chown -R centreon-engine:centreon-engine \
  /etc/centreon-engine \
  /var/lib/centreon-engine \
  /var/lib/centreon-engine/rw \
  /var/log/centreon-engine

chmod 0775 /etc/centreon-engine
chmod 0755 /var/lib/centreon-engine
chmod 0775 /var/lib/centreon-engine/rw
chmod 0755 /var/log/centreon-engine

startCentengine() {
  systemctl daemon-reload ||:
  systemctl unmask centengine.service ||:
  systemctl preset centengine.service ||:
  systemctl enable centengine.service ||:
  systemctl restart centengine.service ||:
}

action="$1"
if  [ "$1" = "configure" ] && [ -z "$2" ]; then
  # Alpine linux does not pass args, and deb passes $1=configure
  action="install"
elif [ "$1" = "configure" ] && [ -n "$2" ]; then
  # deb passes $1=configure $2=<current version>
  action="upgrade"
fi

case "$action" in
  "1" | "install")
    startCentengine
    ;;
  "2" | "upgrade")
    startCentengine
    ;;
  *)
    # $1 == version being installed
    startCentengine
    ;;
esac



#!/bin/sh

startCentengine() {
  systemctl daemon-reload ||:
  systemctl unmask centengine.service ||:
  systemctl preset centengine.service ||:
  systemctl enable centengine.service ||:
  systemctl restart centengine.service ||:
}

debianLinkNagios() {
  if [ ! -r /usr/lib64/nagios/plugins ]; then
    if [ ! -d /usr/lib64/nagios ]; then
      mkdir -p /usr/lib64/nagios
      chmod 0755 /usr/lib64/nagios
    fi
    ln -s /usr/lib/nagios/plugins /usr/lib64/nagios/plugins
  fi
}

createEngineContextFile() {
  FILE="/etc/centreon-engine/engine-context.json"
  if [ ! -f "$FILE" ]; then
    touch "$FILE"
    chmod 660 "$FILE"
    chown centreon-engine:centreon-engine "$FILE"
  fi
}

# on debian, it is needed to recreate centreon-engine user at each upgrade because it is removed on postrm step on versions < 23.10
if [ "$1" = "configure" ] ; then
  if [ ! "$(getent passwd centreon-engine)" ]; then
    adduser --system --group --home /var/lib/centreon-engine --shell /bin/bash --no-create-home centreon-engine
  fi
  if [ "$(getent passwd centreon-broker)" ]; then
    usermod -a -G centreon-engine centreon-broker
    usermod -a -G centreon-broker centreon-engine
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
  chown -R centreon-engine:centreon-engine \
    /etc/centreon-engine \
    /var/lib/centreon-engine \
    /var/log/centreon-engine
  chmod -R g+w \
    /etc/centreon-engine \
    /var/log/centreon-engine
fi

action="$1"
if  [ "$1" = "configure" ] && [ -z "$2" ]; then
  # Alpine linux does not pass args, and deb passes $1=configure
  action="install"
elif [ "$1" = "configure" ] && [ -n "$2" ]; then
  # deb passes $1=configure $2=<current version>
  action="upgrade"
fi

#In debian nagios plugins are stored in /usr/lib/nagios/plugins instead of /usr/lib64/nagios/plugins
#so we create a link /usr/lib/nagios/plugins instead => /usr/lib64/nagios/plugins in order to have
#the same commands configuration for all pollers
if  [ "$1" = "configure" ]; then
  debianLinkNagios
fi

case "$action" in
  "1" | "install")
    createEngineContextFile
    startCentengine
    ;;
  "2" | "upgrade")
    createEngineContextFile
    startCentengine
    ;;
  *)
    # $1 == version being installed
    startCentengine
    ;;
esac



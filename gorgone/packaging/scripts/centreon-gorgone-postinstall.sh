#!/bin/bash

manageUserGroups() {
  # centreon-gorgone must be able to write into shared directories like
  # /var/cache/centreon (owned by centreon:centreon) regardless of whether
  # this is a central or a poller install. This used to be handled by
  # centreon-gorgone-centreon-config's postinstall, but that package is no
  # longer pulled in on a poller (it now depends on centreon-central instead
  # of centreon-gorgone), so it never ran there.
  if getent passwd centreon-engine > /dev/null 2>&1; then
    usermod -a -G centreon-gorgone centreon-engine 2> /dev/null
  fi

  if getent passwd centreon-broker > /dev/null 2>&1; then
    usermod -a -G centreon-gorgone centreon-broker 2> /dev/null
  fi

  if getent passwd centreon-gorgone > /dev/null 2>&1; then
    usermod -a -G centreon centreon-gorgone 2> /dev/null
  fi

  if getent passwd centreon > /dev/null 2>&1; then
    usermod -a -G centreon-gorgone centreon 2> /dev/null
  fi
}

startGorgoned() {
  systemctl daemon-reload ||:
  systemctl unmask gorgoned.service ||:
  systemctl preset gorgoned.service ||:
  systemctl enable gorgoned.service ||:
  systemctl restart gorgoned.service ||:
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
    manageUserGroups
    startGorgoned
    ;;
  "2" | "upgrade")
    manageUserGroups
    startGorgoned
    ;;
  *)
    # $1 == version being installed
    manageUserGroups
    startGorgoned
    ;;
esac

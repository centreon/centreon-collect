#!/bin/bash

startGorgoned() {
  systemctl daemon-reload ||:
  systemctl unmask gorgoned.service ||:
  systemctl preset gorgoned.service ||:
  systemctl enable gorgoned.service ||:
  systemctl restart gorgoned.service ||:
}

# Change the gorgone default log file to be 640, it was 644 before.
# New gorgone version create it as 640 but do not change existing log file.
if [[ -e /var/log/centreon-gorgone/gorgoned.log ]] ; then
  chmod 640 /var/log/centreon-gorgone/gorgoned.log
fi

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
    startGorgoned
    ;;
  "2" | "upgrade")
    startGorgoned
    ;;
  *)
    # $1 == version being installed
    startGorgoned
    ;;
esac

#!/bin/sh

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



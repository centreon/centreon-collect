#!/bin/sh

startCbd() {
  systemctl daemon-reload ||:
  systemctl unmask cbd.service ||:
  systemctl preset cbd.service ||:
  systemctl enable cbd.service ||:
  systemctl restart cbd.service ||:
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
    startCbd
    ;;
  "2" | "upgrade")
    startCbd
    ;;
  *)
    # $1 == version being installed
    startCbd
    ;;
esac

#!/bin/sh

cleanInstall() {
    printf "\033[32m Reload the service unit from disk\033[0m\n"
    systemctl daemon-reload ||:
    printf "\033[32m Unmask the service\033[0m\n"
    systemctl unmask cbd.service ||:
    printf "\033[32m Set the preset flag for the service unit\033[0m\n"
    systemctl preset cbd.service ||:
    printf "\033[32m Set the enabled flag for the service unit\033[0m\n"
    systemctl enable cbd.service ||:
    systemctl restart cbd.service ||:
}

upgrade() {
    printf "\033[32m Reload the service unit from disk\033[0m\n"
    systemctl daemon-reload ||:
    printf "\033[32m Unmask the service\033[0m\n"
    systemctl unmask cbd.service ||:
    printf "\033[32m Set the preset flag for the service unit\033[0m\n"
    systemctl preset cbd.service ||:
    printf "\033[32m Set the enabled flag for the service unit\033[0m\n"
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
    cleanInstall
    ;;
  "2" | "upgrade")
    printf "\033[32m Post Install of an upgrade\033[0m\n"
    upgrade
    ;;
  *)
    # $1 == version being installed
    printf "\033[32m Alpine\033[0m"
    cleanInstall
    ;;
esac



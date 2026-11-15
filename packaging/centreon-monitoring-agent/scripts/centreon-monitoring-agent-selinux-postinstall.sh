#!/bin/sh

. /usr/share/selinux/packages/centreon/centreon-selinux-functions.sh

load() {
  # paths: keep in sync with the module .fc file
  centreon_selinux_load centreon-monitoring-agent /usr/bin/centagent || :
}

action="$1"
if [ "$1" = "configure" ] && [ -z "$2" ]; then
  action="install"
elif [ "$1" = "configure" ] && [ -n "$2" ]; then
  action="upgrade"
fi

case "$action" in
  "1" | "install" | "2" | "upgrade")
    load
    ;;
esac

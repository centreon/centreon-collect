#!/bin/sh

. /usr/share/selinux/packages/centreon/centreon-selinux-functions.sh

load() {
  # paths: keep in sync with the module .fc file
  centreon_selinux_load centreon-common /etc/centreon /var/log/centreon /var/cache/centreon \
    /var/spool/centreon /var/spool/centreontrapd || :
  restorecon -R /run/dbus/system_bus_socket || :
  setsebool -P daemons_enable_cluster_mode on || :
  setsebool -P cluster_can_network_connect on || :
  setsebool -P cluster_manage_all_files on || :
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

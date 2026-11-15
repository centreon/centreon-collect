#!/bin/sh

. /usr/share/selinux/packages/centreon/centreon-selinux-functions.sh

# default ports; custom ones: semanage port -a -t <type> -p tcp <port>
CENTREON_PORTS="centreon_broker_port_t:5669 centreon_broker_port_t:5670 centreon_broker_port_t:51001
  centreon_broker_port_t:51002 centreon_otel_port_t:4317 centreon_gorgoned_port_t:5556 centreon_gorgoned_port_t:8085"

load() {
  # paths: keep in sync with the module .fc file
  centreon_selinux_load centreon-common /etc/centreon /etc/centreon-engine /etc/centreon-broker /var/log/centreon /var/cache/centreon \
    /var/spool/centreon /var/spool/centreontrapd || return
  # shellcheck disable=SC2086
  centreon_selinux_label_ports $CENTREON_PORTS
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

#!/bin/sh

. /usr/share/selinux/packages/centreon/centreon-selinux-functions.sh

# default ports; custom ones: semanage port -a -t <type> -p tcp <port>
CENTREON_PORTS="centreon_broker_port_t:5669 centreon_broker_port_t:5670 centreon_broker_port_t:51001
  centreon_broker_port_t:51002 centreon_otel_port_t:4317 centreon_gorgoned_port_t:5556 centreon_gorgoned_port_t:8085"

# booleans are left as they are: the admin may rely on them for something else
if [ "$1" -lt "1" ]; then
  # the port mappings reference types of the module: delete them first
  # shellcheck disable=SC2086
  centreon_selinux_unlabel_ports $CENTREON_PORTS
  centreon_selinux_unload centreon-common
fi

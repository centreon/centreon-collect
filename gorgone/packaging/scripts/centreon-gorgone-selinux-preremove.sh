#!/bin/sh

. /usr/share/selinux/packages/centreon/centreon-selinux-functions.sh

if [ "$1" -lt "1" ]; then
  centreon_selinux_unload centreon-gorgoned
fi

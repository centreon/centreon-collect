#!/bin/sh

. /usr/share/selinux/packages/centreon/centreon-selinux-functions.sh

# booleans are left as they are: the admin may rely on them for something else
if [ "$1" -lt "1" ]; then
  centreon_selinux_unload centreon-common
fi

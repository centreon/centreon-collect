#!/bin/bash

# Only a real removal may stop the unit: on an upgrade the new package's
# postinstall restarts it unconditionally.
case "$1" in
  1 | [2-9] | [1-9][0-9]* | upgrade | failed-upgrade | deconfigure)
    ;;
  *)
    systemctl stop centreon.service ||:
    ;;
esac

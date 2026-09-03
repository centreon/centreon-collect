#!/bin/sh

# Shared by the rpm %preun (el8, el9, el10) and the deb prerm (bookworm,
# trixie, jammy, noble). Only a real removal may stop the service: on an
# upgrade the new package's postinstall restarts centagent unconditionally.
case "$1" in
  1 | [2-9] | [1-9][0-9]* | upgrade | failed-upgrade | deconfigure)
    ;;
  *)
    systemctl stop centagent.service ||:
    ;;
esac

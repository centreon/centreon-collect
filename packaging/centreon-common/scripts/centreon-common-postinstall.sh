#!/bin/bash

# force 2775 to cache config directories
fixCacheConfigRights() {
  echo "Forcing rights of centreon cache directories ..."
  chmod 2775 /var/cache/centreon/config
  chmod 2775 /var/cache/centreon/config/engine
  chmod 2775 /var/cache/centreon/config/broker
  chmod 2775 /var/cache/centreon/config/export
  chmod 2775 /var/cache/centreon/config/vmware

  # MON-38165 (skip the unexpanded glob of an empty directory)
  for f in /var/cache/centreon/config/engine/* /var/cache/centreon/config/broker/*; do
    [[ -e "${f}" ]] && chmod 0770 "${f}"
  done
}

startCentreon() {
  systemctl daemon-reload ||:
  systemctl unmask centreon.service ||:
  systemctl preset centreon.service ||:
  systemctl enable centreon.service ||:
  systemctl restart centreon.service ||:
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
    fixCacheConfigRights
    startCentreon
    ;;
  "2" | "upgrade")
    fixCacheConfigRights
    startCentreon
    ;;
  *)
    # $1 == version being installed
    startCentreon
    ;;
esac

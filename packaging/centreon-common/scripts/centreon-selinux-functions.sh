# shellcheck shell=sh
# Shared by the Centreon *-selinux package scriptlets (sourced, not executed).

CENTREON_SELINUX_DIR=/usr/share/selinux/packages/centreon
CENTREON_SELINUX_PRIORITY=200

# centreon_selinux_load <module> [path...]: load the module, then relabel the paths of its .fc
centreon_selinux_load() {
  module=$1
  shift
  legacy=""
  # modules used to be installed at the default priority (400), which would shadow priority 200
  if semodule --list-modules=full 2>/dev/null | grep -qE "^400 +$module +"; then
    legacy="-X 400 -r $module"
  fi
  # shellcheck disable=SC2086
  if ! semodule -X "$CENTREON_SELINUX_PRIORITY" -i "$CENTREON_SELINUX_DIR/$module.pp" $legacy; then
    echo "ERROR: could not load the $module SELinux module" >&2
    return 1
  fi
  for path in "$@"; do
    if [ -e "$path" ]; then
      restorecon -RF "$path" || echo "ERROR: could not relabel $path" >&2
    fi
  done
}

# centreon_selinux_unload <module>
centreon_selinux_unload() {
  semodule -X "$CENTREON_SELINUX_PRIORITY" -r "$1" > /dev/null 2>&1 \
    || semodule -r "$1" > /dev/null 2>&1 \
    || echo "ERROR: could not remove the $1 SELinux module" >&2
}

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

# tcp ports that already have a local (semanage) mapping, one per line
centreon_selinux_local_ports() {
  semanage port -l -C -n 2>/dev/null \
    | awk '$2 == "tcp" { for (i = 3; i <= NF; i++) { f = $i; sub(/,$/, "", f); print f } }'
}

# centreon_selinux_label_ports <type:port>...: one semanage transaction for all ports
centreon_selinux_label_ports() {
  existing=$(centreon_selinux_local_ports)
  cmds=""
  for spec in "$@"; do
    action="-a"
    if printf '%s\n' "$existing" | grep -qx "${spec##*:}"; then
      action="-m"
    fi
    cmds="${cmds}port $action -t ${spec%%:*} -p tcp ${spec##*:}
"
  done
  printf '%s' "$cmds" | semanage import || echo "ERROR: could not label the Centreon tcp ports" >&2
}

# centreon_selinux_unlabel_ports <type:port>...: only the mappings still set to our types
centreon_selinux_unlabel_ports() {
  current=$(semanage port -l -C -n 2>/dev/null)
  cmds=""
  for spec in "$@"; do
    if printf '%s\n' "$current" | awk -v t="${spec%%:*}" -v p="${spec##*:}" \
      '$1 == t && $2 == "tcp" { for (i = 3; i <= NF; i++) { f = $i; sub(/,$/, "", f); if (f == p) found = 1 } } END { exit !found }'; then
      cmds="${cmds}port -d -p tcp ${spec##*:}
"
    fi
  done
  [ -z "$cmds" ] || printf '%s' "$cmds" | semanage import || echo "ERROR: could not remove the Centreon tcp port labels" >&2
}

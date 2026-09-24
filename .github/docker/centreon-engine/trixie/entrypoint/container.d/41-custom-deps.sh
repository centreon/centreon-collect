#!/bin/sh

# This script is sourced by container.sh (see 00-init loop), so it runs in the
# parent shell: use `return` (not `exit`, which would kill the whole entrypoint
# before centengine starts) and let container.sh own `set -e`.

DEPS_JSON="/etc/centreon-engine/custom-deps.json"

[ -f "$DEPS_JSON" ] || return 0

. /var/lib/centreon-engine/apt_install.sh

PKGS=$(python3 /var/lib/centreon-engine/check_custom_deps.py "$DEPS_JSON" 2>/dev/null)
if [ -n "$PKGS" ]; then
    echo "Installing custom APT deps: $PKGS"
    # Run in background so centengine starts without waiting for apt.
    # shellcheck disable=SC2086
    apt_install_pkgs $PKGS &
else
    echo "custom-deps.json found but no valid APT packages to install"
fi

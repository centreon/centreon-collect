#!/bin/sh

set -e

DEPS_JSON="/etc/centreon-engine/custom-deps.json"

[ -f "$DEPS_JSON" ] || exit 0

(
    export DEBIAN_FRONTEND=noninteractive
    PKGS=$(python3 /var/lib/centreon-engine/check_custom_deps.py "$DEPS_JSON" 2>/dev/null)
    if [ -n "$PKGS" ]; then
        echo "Installing custom APT deps: $PKGS"
        sudo apt-get update -qq > /proc/1/fd/1 2>&1 || true
        # shellcheck disable=SC2086
        sudo apt-get install -y -- $PKGS > /proc/1/fd/1 2>&1 || true
    else
        echo "custom-deps.json found but no valid APT packages to install"
    fi
) &

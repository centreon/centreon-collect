#!/bin/sh
# Shared helper sourced by the container.d startup/watcher scripts.
# Single source of truth for "install APT packages safely".

APT_LOCK="/var/lock/centreon-engine-apt.lock"

# apt_install_pkgs <pkg> [pkg...]
#
# Serializes against the other concurrent apt callers (40/41/45/46) via flock to
# avoid dpkg lock conflicts, refreshes the package lists, then batch-installs.
#
# Batch install is the fast path and resolves inter-package dependencies in one
# shot. If it fails — a nonexistent name (e.g. "toto"), a purely virtual package,
# or an unsatisfiable dependency — apt aborts the WHOLE transaction, so we fall
# back to installing each package individually. That isolates the bad package and
# lets every valid one still land. The per-package retry does not break shared
# dependencies: each install still pulls what it needs from the repo.
apt_install_pkgs() {
    [ "$#" -gt 0 ] || return 0
    (
        flock -w 120 9 || { echo "apt lock timeout, skipping install" > /proc/1/fd/1; exit 0; }
        export DEBIAN_FRONTEND=noninteractive
        sudo apt-get update -qq > /proc/1/fd/1 2>&1 || true
        if sudo apt-get install -y -- "$@" > /proc/1/fd/1 2>&1; then
            echo "installed: $*" > /proc/1/fd/1
        else
            echo "WARNING: batch install failed, retrying packages individually" > /proc/1/fd/1
            for p in "$@"; do
                if sudo apt-get install -y -- "$p" > /proc/1/fd/1 2>&1; then
                    echo "installed: $p" > /proc/1/fd/1
                else
                    echo "WARNING: skipping unavailable package: $p" > /proc/1/fd/1
                fi
            done
        fi
    ) 9>"${APT_LOCK}"
}

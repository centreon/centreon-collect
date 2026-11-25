#!/bin/bash
# ABBA campaign of the conf-export benchmark: one version against another, on the
# same platform, alternating installs.
#
# Run it in the container, from tests/benchmarks, with the robot venv active:
#
#   ./abba-conf-export.sh /work/bench-91385d6624 abba-ce-91385d6624 /work abba-ce-head
#
# The arguments are the source tree and campaign label of A, then those of B. Each
# tree must hold a build/ made for it: it is installed with cmake --install, from
# the tree itself, as the install rules expect.
#
# The order is warm-up B, then A B B A. The first run of a session is measured
# apart and thrown away -- it is reliably faster than the others, and the ABBA
# cancels a linear drift, not an outlier at the start. (B1 + B2) against
# (A1 + A2) is then unbiased even if the machine warms up or slows down.
#
# Every install is checked against its build, by GNU build id: cbd, centengine, and
# the cbmod.so centengine actually loads -- resolved through ldd, since the copy in
# /usr/lib64/nagios is a leftover nothing refreshes. A mismatch stops the campaign:
# a hybrid install is how a night of measurements once compared nothing.

set -u

if [ $# -ne 4 ] ; then
    echo "usage: $0 <tree A> <label A> <tree B> <label B>" >&2
    exit 2
fi
A_REPO=$1 ; A_LABEL=$2
B_REPO=$3 ; B_LABEL=$4

# Variables handed to the benchmark, the same for every run. Override from the
# environment, e.g. EXPORTS=5 ./abba-conf-export.sh ...
EXPORTS=${EXPORTS:-3}
NB_POLLERS=${NB_POLLERS:-4}
NB_HOSTS=${NB_HOSTS:-1000}
SVC_BY_HOST=${SVC_BY_HOST:-80}
# Three minutes is the measured length of one run at the defaults.
RUN_TIMEOUT=${RUN_TIMEOUT:-1200}

cbmod_path() { ldd /usr/sbin/centengine 2>/dev/null | awk '/cbmod/ {print $3; exit}'; }

# The GNU build id of an ELF file. Not a byte comparison: cmake --install rewrites
# the RPATH of centengine, so the installed file never equals its build, while the
# build id, computed at link time, survives the rewrite.
build_id() { readelf -n "$1" 2>/dev/null | awk '/Build ID/ {print $3; exit}'; }

same_build() {
    local a b
    a=$(build_id "$1") ; b=$(build_id "$2")
    [ -n "$a" ] && [ "$a" = "$b" ]
}

# Whether what is installed is the build of a tree.
installed_from() {
    local repo=$1
    same_build /usr/sbin/cbd "${repo}/build/broker/cbd" &&
        same_build /usr/sbin/centengine "${repo}/build/engine/centengine" &&
        same_build "$(cbmod_path)" "${repo}/build/broker/neb/cbmod.so"
}

install_and_check() {
    local repo=$1 who=$2
    if installed_from "${repo}" ; then
        echo "   ${who}: already installed from ${repo}"
        return 0
    fi
    echo "   ${who}: installing from ${repo}"
    ( cd "${repo}" && cmake --install build ) > /tmp/abba-install.log 2>&1
    if ! installed_from "${repo}" ; then
        echo "FAILED: what is installed is not the build of ${repo}" >&2
        return 1
    fi
    if ! timeout 20 /usr/sbin/centengine --help >/dev/null 2>/tmp/abba-eng.err ; then
        echo "FAILED: centengine does not start: $(head -1 /tmp/abba-eng.err)" >&2
        return 1
    fi
    echo "   ${who}: installed and checked"
}

run_one() {
    local step=$1 who=$2 repo=$3 label=$4
    echo "=== step ${step}: ${who} (${label}) -- $(date +%H:%M) ==="
    install_and_check "${repo}" "${who}" || exit 1
    timeout --kill-after=60 "${RUN_TIMEOUT}" \
        ./bench.py run conf-export \
        --var exports:"${EXPORTS}" \
        --var nb_pollers:"${NB_POLLERS}" \
        --var nb_hosts:"${NB_HOSTS}" \
        --var svc_by_host:"${SVC_BY_HOST}" \
        --label "${label}" --allow-dirty
    local rc=$?
    [ ${rc} -eq 124 ] && echo "   step ${step}: TIMEOUT after ${RUN_TIMEOUT}s" >&2
    # A run that hung leaves daemons behind; the next one would measure them too.
    pkill -9 -f "robotframework/bin/robot" 2>/dev/null
    pkill -9 -f "/usr/sbin/cbd" 2>/dev/null
    pkill -9 -f "/usr/sbin/centengine" 2>/dev/null
    pkill -9 -f "conf_export_probe.py" 2>/dev/null
    if [ ${rc} -ne 0 ] ; then
        echo "stopped: step ${step} failed (rc=${rc})" >&2
        exit 1
    fi
}

run_one 0 B "${B_REPO}" "${B_LABEL}-warmup"
step=0
for who in A B B A ; do
    step=$(( step + 1 ))
    if [ "${who}" = "A" ] ; then
        run_one ${step} A "${A_REPO}" "${A_LABEL}"
    else
        run_one ${step} B "${B_REPO}" "${B_LABEL}"
    fi
done
# Leave B installed, which is what the working tree builds.
install_and_check "${B_REPO}" B || exit 1
echo "=== ABBA done -- $(date +%H:%M) ==="
echo "./bench.py compare ${A_LABEL} ${B_LABEL}"
